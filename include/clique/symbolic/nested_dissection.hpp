/*
   Clique: a scalable implementation of the multifrontal algorithm

   Copyright (C) 2011-2012 Jack Poulson, Lexing Ying, and 
   The University of Texas at Austin
 
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
 
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
 
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef CLIQUE_NESTED_DISSECTION_HPP
#define CLIQUE_NESTED_DISSECTION_HPP 1

#ifdef HAVE_PARMETIS

#include "parmetis.h"

namespace cliq {

// NOTE: This routine is not yet finished
void NestedDissection
( const DistGraph& graph, DistSymmElimTree& eTree, DistSeparatorTree& sepTree );

int Bisect
( const Graph& graph, Graph& leftChild, Graph& rightChild, 
  std::vector<int>& map );

// NOTE: for two or more processes
int Bisect
( const DistGraph& graph, DistGraph& child, 
  std::vector<int>& localMap, bool& haveLeftChild );

void MapIndices
( const std::vector<int>& localMap, 
        std::vector<int>& localIndices, int blocksize, mpi::Comm comm );

// third(i) := second(first(i))
void ComposeMaps
( const std::vector<int>& localFirstMap, 
  const std::vector<int>& localSecondMap,
        std::vector<int>& localThirdMap, int blocksize, mpi::Comm comm );

void InvertMap
( const std::vector<int>& localMap,
        std::vector<int>& localInverseMap, int blocksize, mpi::Comm comm );

int DistributedDepth( mpi::Comm comm );

//----------------------------------------------------------------------------//
// Implementation begins here                                                 //
//----------------------------------------------------------------------------//

inline void
DistributedDepthRecursion
( unsigned commRank, unsigned commSize, unsigned& distDepth )
{
    if( commSize == 1 )
        return;

    ++distDepth;
    const unsigned smallTeamSize = commSize/2;
    const unsigned largeTeamSize = commSize - smallTeamSize;
    if( commRank < smallTeamSize )
        DistributedDepthRecursion( commRank, smallTeamSize, distDepth );
    else
        DistributedDepthRecursion
        ( commRank-smallTeamSize, largeTeamSize, distDepth );
}

inline int
DistributedDepth( mpi::Comm comm )
{
    unsigned commRank = mpi::CommRank( comm );
    unsigned commSize = mpi::CommSize( comm );
    unsigned distDepth = 0;
    DistributedDepthRecursion( commRank, commSize, distDepth );
    return distDepth;
}

inline void
NestedDissectionRecursion
( const DistGraph& graph, DistSymmElimTree& eTree, DistSeparatorTree& sepTree,
  int depth )
{
#ifndef RELEASE
    PushCallStack("NestedDissectionRecursion");
#endif
    const int distDepth = sepTree.distSeps.size();
    if( distDepth - depth > 0 )
    {
        DistGraph child;
        bool haveLeftChild;
        std::vector<int> localMap;
        const int sepSize = Bisect( graph, child, localMap, haveLeftChild );

        NestedDissectionRecursion( child, eTree, sepTree, depth+1 );
        // TODO
    }
    else
    {
        Graph seqGraph( graph );

        Graph leftChild, rightChild;
        std::vector<int> map;
        const int sepSize = Bisect( seqGraph, leftChild, rightChild, map );

        // TODO
        //NestedDissectionRecursion( leftChild, eTree, sepTree );
        //NestedDissectionRecursion( rightChild, eTree, sepTree );
    }
#ifndef RELEASE
    PopCallStack();
#endif
}

inline void 
NestedDissection
( const DistGraph& graph, DistSymmElimTree& eTree, DistSeparatorTree& sepTree )
{
#ifndef RELEASE
    PushCallStack("NestedDissection");
#endif
    mpi::Comm comm = graph.Comm();
    const int commRank = mpi::CommRank( comm );
    const int commSize = mpi::CommSize( comm );

    const int distDepth = DistributedDepth( comm );

    eTree.distNodes.resize( distDepth+1 );
    sepTree.distSeps.resize( distDepth );

    NestedDissectionRecursion( graph, eTree, sepTree, 0 );
    // TODO
#ifndef RELEASE
    PopCallStack();
#endif
}

inline int 
Bisect
( const Graph& graph ,Graph& leftChild, Graph& rightChild,
  std::vector<int>& map )
{
#ifndef RELEASE
    PushCallStack("Bisect");
#endif
    // TODO: Use a wrapper to METIS instead of ParMETIS

    // Describe the source distribution
    const int numSources = graph.NumSources();
    std::vector<idx_t> vtxDist( 2 );
    vtxDist[0] = 0;
    vtxDist[1] = numSources;

    // ParMETIS assumes that there are no self-connections, so we must
    // manually remove them from our graph
    const int numEdges = graph.NumEdges();
    int numSelfEdges = 0;
    for( int i=0; i<numEdges; ++i )
        if( graph.Source(i) == graph.Target(i) )
            ++numSelfEdges;

    // Fill our connectivity (ignoring self edges)
    const int numValidEdges = numEdges - numSelfEdges;
    std::vector<idx_t> xAdj( numSources+1 );
    std::vector<idx_t> adjacency( numValidEdges );
    int validCounter=0;
    int sourceOffset=0;
    int prevSource=-1;
    for( int edge=0; edge<numEdges; ++edge )
    {
        const int source = graph.Source( edge );
        const int target = graph.Target( edge );
#ifndef RELEASE
        if( source < prevSource )
            throw std::runtime_error("sources were not properly sorted");
#endif
        while( source != prevSource )
        {
            xAdj[sourceOffset++] = validCounter;
            ++prevSource;
        }
        if( source != target )
        {
            adjacency[validCounter] = target;
            ++validCounter;
        }
    }
#ifndef RELEASE
    if( sourceOffset != numSources )
        throw std::logic_error("Mistake in xAdj computation");
#endif
    xAdj[numSources] = numValidEdges;

    // Create space for the result
    map.resize( numSources );

    // Use the custom ParMETIS interface
    mpi::Comm comm = mpi::COMM_SELF;
    idx_t numParSeps = 10;
    idx_t numSeqSeps = 5;
    real_t imbalance = 1.1;
    idx_t sizes[3];
    const int retval = CliqBisect
    ( &vtxDist[0], &xAdj[0], &adjacency[0], &numParSeps, &numSeqSeps, 
      &imbalance, NULL, &map[0], sizes, &comm );
#ifndef RELEASE
    std::vector<int> timesMapped( numSources, 0 );
    for( int i=0; i<numSources; ++i )
        ++timesMapped[map[i]];
    for( int i=0; i<numSources; ++i )
    {
        if( timesMapped[i] != 1 )
        {
            std::ostringstream msg;
            msg << timesMapped[i] << " vertices were relabeled as "
                << i;
            throw std::logic_error( msg.str().c_str() );
        }
    }
#endif

    const int leftChildSize = sizes[0];
    const int rightChildSize = sizes[1];
    const int sepSize = sizes[2];

    // Build the inverse map
    std::vector<int> inverseMap( numSources );
    for( int i=0; i<numSources; ++i )
        inverseMap[map[i]] = i;

    // Get an upper bound on the number of edges in the child graphs
    int leftChildUpperBound=0, rightChildUpperBound=0;
    for( int s=0; s<leftChildSize; ++s )
        leftChildUpperBound += graph.NumConnections( inverseMap[s] );
    for( int s=0; s<rightChildSize; ++s )
        rightChildUpperBound += 
            graph.NumConnections( inverseMap[s+leftChildSize] );

    // Build the left child's graph
    leftChild.ResizeTo( leftChildSize );
    leftChild.StartAssembly();
    leftChild.Reserve( leftChildUpperBound );
    for( int s=0; s<leftChildSize; ++s )
    {
        const int source = s;
        const int inverseSource = inverseMap[s];
        const int offset = graph.EdgeOffset( inverseSource );
        const int numConnections = graph.NumConnections( inverseSource );
        for( int t=0; t<numConnections; ++t )
        {
            const int inverseTarget = graph.Target( offset+t );
            const int target = map[inverseTarget];
            leftChild.PushBack( source, target );
        }
    }
    leftChild.StopAssembly();

    // Build the right child's graph
    rightChild.ResizeTo( rightChildSize );
    rightChild.StartAssembly();
    rightChild.Reserve( rightChildUpperBound );
    for( int s=0; s<rightChildSize; ++s )
    {
        const int source = s+leftChildSize;
        const int inverseSource = inverseMap[source];
        const int offset = graph.EdgeOffset( inverseSource );
        const int numConnections = graph.NumConnections( inverseSource );
        for( int t=0; t<numConnections; ++t )
        {
            const int inverseTarget = graph.Target( offset+t );
            const int target = map[inverseTarget];
            rightChild.PushBack( source-leftChildSize, target-leftChildSize );
        }
    }
    rightChild.StopAssembly();
#ifndef RELEASE
    PopCallStack();
#endif
    return sepSize;
}

inline int 
Bisect
( const DistGraph& graph, DistGraph& child, 
  std::vector<int>& localMap, bool& haveLeftChild )
{
#ifndef RELEASE
    PushCallStack("Bisect");
#endif
    mpi::Comm comm = graph.Comm();
    const int commSize = mpi::CommSize( comm );
    const int commRank = mpi::CommRank( comm );
    if( commSize == 1 )
        throw std::logic_error
        ("This routine assumes at least two processes are used, "
         "otherwise one child will be lost");

    // Describe the source distribution
    const int blocksize = graph.Blocksize();
    std::vector<idx_t> vtxDist( commSize+1 );
    for( int i=0; i<commSize; ++i )
        vtxDist[i] = i*blocksize;
    vtxDist[commSize] = graph.NumSources();

    // ParMETIS assumes that there are no self-connections, so we must
    // manually remove them from our graph
    const int numLocalEdges = graph.NumLocalEdges();
    int numLocalSelfEdges = 0;
    for( int i=0; i<numLocalEdges; ++i )
        if( graph.Source(i) == graph.Target(i) )
            ++numLocalSelfEdges;

    // Fill our local connectivity (ignoring self edges)
    const int numLocalValidEdges = numLocalEdges - numLocalSelfEdges;
    const int numLocalSources = graph.NumLocalSources();
    const int firstLocalSource = graph.FirstLocalSource();
    std::vector<idx_t> xAdj( numLocalSources+1 );
    std::vector<idx_t> adjacency( numLocalValidEdges );
    int validCounter=0;
    int sourceOffset=0;
    int prevSource=firstLocalSource-1;
    for( int localEdge=0; localEdge<numLocalEdges; ++localEdge )
    {
        const int source = graph.Source( localEdge );
        const int target = graph.Target( localEdge );
#ifndef RELEASE
        if( source < prevSource )
            throw std::runtime_error("sources were not properly sorted");
#endif
        while( source != prevSource )
        {
            xAdj[sourceOffset++] = validCounter;
            ++prevSource;
        }
        if( source != target )
        {
            adjacency[validCounter] = target;
            ++validCounter;
        }
    }
#ifndef RELEASE
    if( sourceOffset != numLocalSources )
        throw std::logic_error("Mistake in xAdj computation");
#endif
    xAdj[numLocalSources] = numLocalValidEdges;

    // Create space for the result
    localMap.resize( numLocalSources );

    // Use the custom ParMETIS interface
    idx_t numParSeps = 10;
    idx_t numSeqSeps = 5;
    real_t imbalance = 1.1;
    idx_t sizes[3];
    const int retval = CliqBisect
    ( &vtxDist[0], &xAdj[0], &adjacency[0], &numParSeps, &numSeqSeps, 
      &imbalance, NULL, &localMap[0], sizes, &comm );
#ifndef RELEASE
    const int numSources = graph.NumSources();
    std::vector<int> timesMapped( numSources, 0 );
    for( int i=0; i<numLocalSources; ++i )
        ++timesMapped[localMap[i]];
    mpi::Reduce( &timesMapped[0], numSources, MPI_SUM, 0, comm );
    if( commRank == 0 )
    {
        for( int i=0; i<numSources; ++i )
        {
            if( timesMapped[i] != 1 )
            {
                std::ostringstream msg;
                msg << timesMapped[i] << " vertices were relabeled as "
                    << i;
                throw std::logic_error( msg.str().c_str() );
            }
        }
    }
#endif

    const int leftChildSize = sizes[0];
    const int rightChildSize = sizes[1];
    const int sepSize = sizes[2];

    // Build the child graph from the partitioned parent
    const int smallTeamSize = commSize/2;
    const int largeTeamSize = commSize - smallTeamSize;
    const bool inSmallTeam = ( commRank < smallTeamSize );
    const bool smallOnLeft = ( leftChildSize <= rightChildSize );
    const int leftTeamSize = ( smallOnLeft ? smallTeamSize : largeTeamSize );
    const int rightTeamSize = ( smallOnLeft ? largeTeamSize : smallTeamSize );
    const int leftTeamOffset = ( smallOnLeft ? 0 : smallTeamSize );
    const int rightTeamOffset = ( smallOnLeft ? smallTeamSize : 0 );
    const int leftTeamBlocksize = leftChildSize / leftTeamSize;
    const int rightTeamBlocksize = rightChildSize / rightTeamSize;
    const bool inLeftTeam = ( smallOnLeft == inSmallTeam );

    // Count how many rows we must send to each process 
    std::vector<int> rowSendSizes( commSize, 0 );
    for( int s=0; s<numLocalSources; ++s )
    {
        const int i = localMap[s];
        if( i < leftChildSize )
        {
            const int q = leftTeamOffset + 
                std::min(i/leftTeamBlocksize,leftTeamSize-1);
            ++rowSendSizes[q];
        }
        else if( i < leftChildSize+rightChildSize )
        {
            const int q = 
                rightTeamOffset + 
                std::min((i-leftChildSize)/rightTeamBlocksize,rightTeamSize-1);
            ++rowSendSizes[q];
        }
    }

    // Exchange the number of rows
    std::vector<int> rowRecvSizes( commSize );
    mpi::AllToAll
    ( &rowSendSizes[0], 1,
      &rowRecvSizes[0], 1, comm );

    // Prepare for the AllToAll to exchange the row indices and 
    // the number of column indices per row
    int numSendRows=0;
    std::vector<int> rowSendOffsets( commSize );
    for( int q=0; q<commSize; ++q )
    {
        rowSendOffsets[q] = numSendRows;
        numSendRows += rowSendSizes[q];
    }
    int numRecvRows=0;
    std::vector<int> rowRecvOffsets( commSize );
    for( int q=0; q<commSize; ++q )
    {
        rowRecvOffsets[q] = numRecvRows;
        numRecvRows += rowRecvSizes[q];
    }

    // Pack the row indices and how many column entries there will be per row
    std::vector<int> rowSendLengths( numSendRows );
    std::vector<int> rowSendIndices( numSendRows );
    std::vector<int> offsets = rowSendOffsets;
    for( int s=0; s<numLocalSources; ++s )
    {
        const int i = localMap[s];
        if( i < leftChildSize )
        {
            const int q = leftTeamOffset + 
                std::min(i/leftTeamBlocksize,leftTeamSize-1);
            rowSendIndices[offsets[q]] = i;
            rowSendLengths[offsets[q]] = graph.NumConnections( s );
            ++offsets[q];
        }
        else if( i < leftChildSize+rightChildSize )
        {
            const int q = 
                rightTeamOffset + 
                std::min((i-leftChildSize)/rightTeamBlocksize,rightTeamSize-1);
            rowSendIndices[offsets[q]] = i;
            rowSendLengths[offsets[q]] = graph.NumConnections( s );
            ++offsets[q];
        }
    }

    // Perform the row lengths exchange
    std::vector<int> rowRecvLengths( numRecvRows );
    mpi::AllToAll
    ( &rowSendLengths[0], &rowSendSizes[0], &rowSendOffsets[0],
      &rowRecvLengths[0], &rowRecvSizes[0], &rowRecvOffsets[0], comm );
    rowSendLengths.clear();

    // Perform the row indices exchange
    std::vector<int> rowRecvIndices( numRecvRows );
    mpi::AllToAll
    ( &rowSendIndices[0], &rowSendSizes[0], &rowSendOffsets[0],
      &rowRecvIndices[0], &rowRecvSizes[0], &rowRecvOffsets[0], comm );
    rowSendIndices.clear();
    rowSendSizes.clear();
    rowSendOffsets.clear();

    // Set up for sending the column indices
    int numSendIndices=0;
    std::vector<int> indexSendSizes( commSize, 0 );
    std::vector<int> indexSendOffsets( commSize );
    for( int q=0; q<commSize; ++q )
    {
        const int numRows = rowSendSizes[q];
        const int offset = rowSendOffsets[q];
        for( int s=0; s<numRows; ++s )
            indexSendSizes[q] += rowSendLengths[offset+s];

        indexSendOffsets[q] = numSendIndices;
        numSendIndices += indexSendSizes[q];
    }
    int numRecvIndices=0;
    std::vector<int> indexRecvSizes( commSize, 0 );
    std::vector<int> indexRecvOffsets( commSize );
    for( int q=0; q<commSize; ++q )
    {
        const int numRows = rowRecvSizes[q];
        const int offset = rowRecvOffsets[q];
        for( int s=0; s<numRows; ++s )
            indexRecvSizes[q] += rowRecvLengths[offset+s];

        indexRecvOffsets[q] = numRecvIndices;
        numRecvIndices += indexRecvSizes[q];
    }

    // Pack the indices
    std::vector<int> sendIndices( numSendIndices );
    offsets = indexSendOffsets;
    for( int s=0; s<numLocalSources; ++s )
    {
        const int i = localMap[s];
        if( i < leftChildSize )
        {
            const int q = leftTeamOffset + 
                std::min(i/leftTeamBlocksize,leftTeamSize-1);

            int& offset = offsets[q];
            const int numConnections = graph.NumConnections( s );
            const int localEdgeOffset = graph.LocalEdgeOffset( s );
            for( int j=0; j<numConnections; ++j )
                sendIndices[offset++] = graph.Target( localEdgeOffset+j );
        }
        else if( i < leftChildSize+rightChildSize )
        {
            const int q =
                rightTeamOffset + 
                std::min((i-leftChildSize)/rightTeamBlocksize,rightTeamSize-1);
               
            int& offset = offsets[q];
            const int numConnections = graph.NumConnections( s );
            const int localEdgeOffset = graph.LocalEdgeOffset( s );
            for( int j=0u; j<numConnections; ++j )
                sendIndices[offset++] = graph.Target( localEdgeOffset+j );
        }
    }

    // Send/recv the column indices
    std::vector<int> recvIndices( numRecvIndices );
    mpi::AllToAll
    ( &sendIndices[0], &indexSendSizes[0], &indexSendOffsets[0],
      &recvIndices[0], &indexRecvSizes[0], &indexRecvOffsets[0], comm );
    sendIndices.clear();
    indexSendSizes.clear();
    indexSendOffsets.clear();

    // Get the indices after reordering
    MapIndices( localMap, recvIndices, blocksize, comm );

    // Put the connections into our new graph
    const int childTeamRank = 
        ( inLeftTeam ? commRank-leftTeamOffset : commRank-rightTeamOffset );
    MPI_Comm childComm;
    mpi::CommSplit( comm, inLeftTeam, childTeamRank, childComm );
    child.SetComm( childComm );
    if( inLeftTeam )
    {
        haveLeftChild = true;
        child.ResizeTo( leftChildSize );
    }
    else
    {
        haveLeftChild = false;
        child.ResizeTo( rightChildSize );
    }
    const int childFirstLocalSource = child.FirstLocalSource();
    child.StartAssembly();
    child.Reserve( recvIndices.size() );
    int offset=0;
    for( int s=0; s<numRecvRows; ++s )
    {
        const int source = rowRecvIndices[s];
        const int numConnections = rowRecvLengths[s];
        for( int t=0; t<numConnections; ++t )
        {
            const int target = recvIndices[offset++];
            if( haveLeftChild )
                child.PushBack( source, target );
            else
                child.PushBack( source-leftChildSize, target-leftChildSize );
        }
    }
    child.StopAssembly();
#ifndef RELEASE
    PopCallStack();
#endif
    return sepSize;
}

// Overwrite the array of indices with the distributed map defined by each 
// processes's localMap. 
inline void 
MapIndices
( const std::vector<int>& localMap, 
        std::vector<int>& localIndices, int blocksize, mpi::Comm comm )
{
#ifndef RELEASE
    PushCallStack("MapIndices");
#endif
    const int commRank = mpi::CommRank( comm );
    const int commSize = mpi::CommSize( comm );

    const int firstLocalSource = blocksize*commRank;
    const int numLocalSources = localMap.size();
    const int numLocalIndices = localIndices.size();

    // Count how many indices we need each process to map
    std::vector<int> requestSizes( commSize, 0 );
    for( int s=0; s<numLocalIndices; ++s )
    {
        const int i = localIndices[s];
#ifndef RELEASE
        if( i < 0 )
            throw std::logic_error("Index was negative");
#endif
        const int q = std::min( i/blocksize, commSize-1 );
        ++requestSizes[q];
    }

    // Send our requests and find out what we need to fulfill
    std::vector<int> fulfillSizes( commSize );
    mpi::AllToAll
    ( &requestSizes[0], 1, 
      &fulfillSizes[0], 1, comm );

    // Prepare for the AllToAll to exchange request sizes
    int numRequests=0;
    std::vector<int> requestOffsets( commSize );
    for( int q=0; q<commSize; ++q )
    {
        requestOffsets[q] = numRequests;
        numRequests += requestSizes[q];
    }
#ifndef RELEASE
    if( numRequests != numLocalIndices )
        throw std::logic_error("Miscalculated numRequests");
#endif
    int numFulfills=0;
    std::vector<int> fulfillOffsets( commSize );
    for( int q=0; q<commSize; ++q )
    {
        fulfillOffsets[q] = numFulfills;
        numFulfills += fulfillSizes[q];
    }

    // Pack the requested information 
    std::vector<int> requests( numRequests );
    std::vector<int> offsets = requestOffsets;
    for( int s=0; s<numLocalIndices; ++s )
    {
        const int i = localIndices[s];
        const int q = std::min( i/blocksize, commSize-1 );
        requests[offsets[q]++] = i;
    }

    // Perform the first index exchange
    std::vector<int> fulfills( numFulfills );
    mpi::AllToAll
    ( &requests[0], &requestSizes[0], &requestOffsets[0],
      &fulfills[0], &fulfillSizes[0], &fulfillOffsets[0], comm );

    // Map all of the indices in 'fulfills'
    for( int s=0; s<numFulfills; ++s )
    {
        const int i = fulfills[s];
        const int iLocal = i - firstLocalSource;
#ifndef RELEASE
        if( iLocal < 0 || iLocal >= numLocalSources )
        {
            std::ostringstream msg;
            msg << "invalid request: i=" << i << ", iLocal=" << iLocal 
                << ", commRank=" << commRank << ", blocksize=" << blocksize;
            throw std::logic_error( msg.str().c_str() );
        }
#endif
        fulfills[s] = localMap[iLocal];
    }

    // Send everything back
    mpi::AllToAll
    ( &fulfills[0], &fulfillSizes[0], &fulfillOffsets[0],
      &requests[0], &requestSizes[0], &requestOffsets[0], comm );

    // Unpack in the same way we originally packed
    offsets = requestOffsets;
    for( int s=0; s<numLocalIndices; ++s )
    {
        const int i = localIndices[s];
        const int q = std::min( i/blocksize, commSize-1 );
        localIndices[s] = requests[offsets[q]++];
    }
#ifndef RELEASE
    PopCallStack();
#endif
}

// third(i) := second(first(i))
inline void 
ComposeMaps
( const std::vector<int>& localFirstMap, 
  const std::vector<int>& localSecondMap,
        std::vector<int>& localThirdMap, int blocksize, mpi::Comm comm )
{
#ifndef RELEASE
    PushCallStack("ComposeMaps");
#endif
    localThirdMap = localFirstMap;
    MapIndices( localSecondMap, localThirdMap, blocksize, comm );
#ifndef RELEASE
    PopCallStack();
#endif
}

// Generate our local portion of the inverse of a distributed map
inline void 
InvertMap
( const std::vector<int>& localMap, 
        std::vector<int>& localInverseMap, int blocksize, mpi::Comm comm )
{
#ifndef RELEASE
    PushCallStack("InvertMap");
#endif
    const int commRank = mpi::CommRank( comm );
    const int commSize = mpi::CommSize( comm );

    const int firstLocalSource = blocksize*commSize;
    const int numLocalSources = localMap.size();

    // How many pairs of original and mapped indices to send to each process
    std::vector<int> sendSizes( commSize, 0 );
    for( int s=0; s<numLocalSources; ++s )
    {
        const int i = localMap[s];
        const int q = std::min( i/blocksize, commSize-1 );
        sendSizes[q] += 2;
    }

    // Coordinate all of the processes on their send sizes
    std::vector<int> recvSizes( commSize );
    mpi::AllToAll
    ( &sendSizes[0], 1,
      &recvSizes[0], 1, comm );

    // Prepare for the AllToAll to exchange send sizes
    int numSends=0;
    std::vector<int> sendOffsets( commSize );
    for( int q=0; q<commSize; ++q )
    {
        sendOffsets[q] = numSends;
        numSends += sendSizes[q];
    }
#ifndef RELEASE
    if( numSends != 2*numLocalSources )
        throw std::logic_error("Miscalculated numSends");
#endif
    int numReceives=0;
    std::vector<int> recvOffsets( commSize );
    for( int q=0; q<commSize; ++q )
    {
        recvOffsets[q] = numReceives;
        numReceives += recvSizes[q];
    }
#ifndef RELEASE
    if( numReceives != 2*numLocalSources )
        throw std::logic_error("Mistake in number of receives");
#endif

    // Pack our map information
    std::vector<int> sends( numSends );
    std::vector<int> offsets = sendOffsets;
    for( int s=0; s<numLocalSources; ++s )
    {
        const int i = localMap[s];
        const int q = std::min( i/blocksize, commSize-1 );
        sends[offsets[q]++] = s+firstLocalSource;
        sends[offsets[q]++] = i;
    }

    // Send out the map information
    std::vector<int> recvs( numReceives );
    mpi::AllToAll
    ( &sends[0], &sendSizes[0], &sendOffsets[0],
      &recvs[0], &recvSizes[0], &recvOffsets[0], comm );

    // Form our part of the inverse map
    localInverseMap.resize( numLocalSources );
    for( int s=0; s<numReceives; s+=2 )
    {
        const int origIndex = recvs[s];
        const int mappedIndex = recvs[s+1];
        localInverseMap[mappedIndex-firstLocalSource] = origIndex;
    }
#ifndef RELEASE
    PopCallStack();
#endif
}

} // namespace cliq

#endif // HAVE_PARMETIS

#endif // CLIQUE_NESTED_DISSECTION_HPP
