/*
   Copyright (c) 2009-2013, Jack Poulson, Lexing Ying,
   The University of Texas at Austin, and Stanford University
   All rights reserved.
 
   This file is part of Clique and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "clique.hpp"
using namespace cliq;

int
main( int argc, char* argv[] )
{
    cliq::Initialize( argc, argv );
    mpi::Comm comm = mpi::COMM_WORLD;
    const int commRank = mpi::CommRank( comm );
    const int commSize = mpi::CommSize( comm );

    try
    {
        const int n = Input("--n","size of n x n x n grid",30);
        const bool sequential = Input
            ("--sequential","sequential partitions?",true);
        const int numDistSeps = Input
            ("--numDistSeps",
             "number of separators to try per distributed partition",1);
        const int numSeqSeps = Input
            ("--numSeqSeps",
             "number of separators to try per sequential partition",1);
        const bool print = Input("--print","print graph?",false);
        const bool display = Input("--display","display graph?",false);
        ProcessInput();

        const int numVertices = n*n*n;
        DistGraph graph( numVertices, comm );

        // Fill our portion of the graph of a 3D n x n x n 7-point stencil
        // in natural ordering: (x,y,z) at x + y*n + z*n*n
        const int firstLocalSource = graph.FirstLocalSource();
        const int numLocalSources = graph.NumLocalSources();
        graph.StartAssembly();
        graph.Reserve( 7*numLocalSources );
        for( int iLocal=0; iLocal<numLocalSources; ++iLocal )
        {
            const int i = firstLocalSource + iLocal;
            const int x = i % n;
            const int y = (i/n) % n;
            const int z = i/(n*n);

            graph.Insert( i, i );
            if( x != 0 )
                graph.Insert( i, i-1 );
            if( x != n-1 )
                graph.Insert( i, i+1 );
            if( y != 0 )
                graph.Insert( i, i-n );
            if( y != n-1 )
                graph.Insert( i, i+n );
            if( z != 0 )
                graph.Insert( i, i-n*n );
            if( z != n-1 )
                graph.Insert( i, i+n*n );
        }
        graph.StopAssembly();
        if( display )
            Display( graph );
        if( print )
            Print( graph );

        if( commSize > 1 )
        {
            DistGraph child;
            DistMap map;
            bool haveLeftChild;
            const int sepSize = 
                Bisect
                ( graph, child, map, haveLeftChild, 
                  sequential, numDistSeps, numSeqSeps );

            int leftChildSize, rightChildSize;
            if( haveLeftChild )
            {
                leftChildSize = child.NumSources();
                rightChildSize = numVertices - leftChildSize - sepSize;
            }
            else
            {
                rightChildSize = child.NumSources();
                leftChildSize = numVertices - rightChildSize - sepSize;
            }
            if( commRank == 0 )
            {
                if( haveLeftChild )
                    std::cout << "Root is on left with sizes: " 
                              << leftChildSize << ", " << rightChildSize << ", "
                              << sepSize << std::endl;
                else
                    std::cout << "Root is on right with sizes: " 
                              << leftChildSize << ", " << rightChildSize << ", "
                              << sepSize << std::endl;
            }
        }
        else
        {
            // Turn the single-process DistGraph into a Graph
            Graph seqGraph( graph );

            Graph leftChild, rightChild;
            std::vector<int> map;
            const int sepSize = 
                Bisect( seqGraph, leftChild, rightChild, map, numSeqSeps );

            const int leftChildSize = leftChild.NumSources();
            const int rightChildSize = rightChild.NumSources();
            std::cout << "Partition sizes were: "
                      << leftChildSize << ", " << rightChildSize << ", "
                      << sepSize << std::endl;
        }
    }
    catch( std::exception& e ) { ReportException(e); }

    cliq::Finalize();
    return 0;
}
