/*
   Clique: a scalable implementation of the multifrontal algorithm

   Copyright (C) 2010-2011 Jack Poulson <jack.poulson@gmail.com>
   Copyright (C) 2011 Jack Poulson, Lexing Ying, and 
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
#include "clique.hpp"
using namespace elemental;

// This is the part of the symbolic factorization that requires fine-grain 
// parallelism: we assume that the upper floor(log2(commSize)) levels of the
// tree are balanced.
//
// TODO: Generalize so that the depth can be less than or equal to 
// floor(log2(commSize)). This would allow for the use of more processes in the
// factorization.
//
// TODO: Generalize to support more than just a power-of-two number of 
//       processes. This should be relatively straightforward.
void clique::symbolic::DistSymmetricFactorization
( const DistSymmOrig&  distOrig,
  const LocalSymmFact& localFact, DistSymmFact&  distFact, 
        bool storeFactRecvIndices )
{
#ifndef RELEASE
    PushCallStack("symbolic::DistSymmetricFactorization");
#endif
    const unsigned numSupernodes = distOrig.supernodes.size();
    distFact.supernodes.resize( numSupernodes );
    if( numSupernodes == 0 )
        return;

    const unsigned commRank = mpi::CommRank( distOrig.comm );
    const unsigned commSize = mpi::CommSize( distOrig.comm );
#ifndef RELEASE
    // Use the naive algorithm for finding floor(log2(commSize)) since it
    // will be an ignorable portion of the overhead and will be more easily
    // extended to more general integer types.
    unsigned temp = commSize;
    unsigned log2CommSize = 0;
    while( temp >>= 1 )
        ++log2CommSize;
    if( log2CommSize+1 != numSupernodes )
        throw std::runtime_error("Invalid distributed tree depth");
    if( 1u<<log2CommSize != commSize )
        throw std::runtime_error
        ("Power-of-two number of procs currently required");
#endif

    // The bottom node is already computed, so just copy it over
    const LocalSymmFactSupernode& topLocalSN = localFact.supernodes.back();
    DistSymmFactSupernode& bottomDistSN = distFact.supernodes[0];
    mpi::CommSplit( distOrig.comm, commRank, 0, bottomDistSN.comm );
    bottomDistSN.grid = new elemental::Grid( bottomDistSN.comm, 1, 1 );
    bottomDistSN.size = topLocalSN.size;
    bottomDistSN.localSize1d = topLocalSN.size;
    bottomDistSN.offset = topLocalSN.offset;
    bottomDistSN.myOffset = topLocalSN.myOffset;
    bottomDistSN.localOffset1d = topLocalSN.myOffset;
    bottomDistSN.lowerStruct = topLocalSN.lowerStruct;
    bottomDistSN.origLowerRelIndices = topLocalSN.origLowerRelIndices;
    bottomDistSN.leftChildRelIndices = topLocalSN.leftChildRelIndices;
    bottomDistSN.rightChildRelIndices = topLocalSN.rightChildRelIndices;
    bottomDistSN.leftChildSize = -1; // not needed, could compute though
    bottomDistSN.rightChildSize = -1; // not needed, could compute though
    bottomDistSN.leftChildFactColIndices.clear();
    bottomDistSN.leftChildFactRowIndices.clear();
    bottomDistSN.rightChildFactColIndices.clear();
    bottomDistSN.rightChildFactRowIndices.clear();
    bottomDistSN.numChildFactSendIndices.clear();
    bottomDistSN.childFactRecvIndices.clear();
    bottomDistSN.localOffset1d = topLocalSN.myOffset;
    bottomDistSN.leftChildSolveIndices.clear();
    bottomDistSN.rightChildSolveIndices.clear();
    bottomDistSN.childSolveRecvIndices.clear();

    // Perform the distributed part of the symbolic factorization
    int myOffset = bottomDistSN.myOffset + bottomDistSN.size;
    int localOffset1d = bottomDistSN.localOffset1d + bottomDistSN.size;
    std::vector<int>::iterator it;
    std::vector<int> sendBuffer, recvBuffer;
    std::vector<int> childrenStruct, partialStruct, fullStruct,
                    supernodeIndices;
    for( unsigned s=1; s<numSupernodes; ++s )
    {
        const DistSymmOrigSupernode& origSN = distOrig.supernodes[s];
        const DistSymmFactSupernode& factChildSN = distFact.supernodes[s-1];
        DistSymmFactSupernode& factSN = distFact.supernodes[s];
        factSN.size = origSN.size;
        factSN.offset = origSN.offset;
        factSN.myOffset = myOffset;

        // Determine our partner based upon the bits of 'commRank'
        const unsigned powerOfTwo = 1u << (s-1);
        const unsigned partner = commRank ^ powerOfTwo; // flip the s-1'th bit
        const bool onLeft = (commRank & powerOfTwo) == 0; // check s-1'th bit 

        // Create this level's communicator
        const unsigned teamSize  = powerOfTwo << 1;
        const int teamColor = commRank & ~(teamSize-1);
        const int teamRank  = commRank &  (teamSize-1);
        mpi::CommSplit( distOrig.comm, teamColor, teamRank, factSN.comm );
        unsigned gridHeight = 
            static_cast<unsigned>(sqrt(static_cast<double>(teamSize)));
        while( teamSize % gridHeight != 0 )
            ++gridHeight;
        const unsigned gridWidth = teamSize / gridHeight;
        factSN.grid = new elemental::Grid( factSN.comm, gridHeight, gridWidth );
        const unsigned gridRow = factSN.grid->MCRank();
        const unsigned gridCol = factSN.grid->MRRank();

        // Set some offset and size information for this supernode
        factSN.localSize1d = LocalLength( origSN.size, teamRank, teamSize );
        factSN.localOffset1d = localOffset1d;

        // Retrieve the child grid information
        const unsigned childTeamRank = mpi::CommRank( factChildSN.comm );
        const unsigned childTeamSize = mpi::CommSize( factChildSN.comm );
        const unsigned childGridHeight = factChildSN.grid->Height();
        const unsigned childGridWidth = factChildSN.grid->Width();
        const unsigned childGridRow = factChildSN.grid->MCRank();
        const unsigned childGridCol = factChildSN.grid->MRRank();

        // SendRecv the message lengths
        const int myChildSize = factChildSN.size;
        const int myChildLowerStructSize = factChildSN.lowerStruct.size();
        const int initialSends[2] = { myChildSize, myChildLowerStructSize };
        int initialRecvs[2];
        mpi::SendRecv
        ( initialSends, 2, partner, 0,
          initialRecvs, 2, partner, 0, distOrig.comm );
        const int theirChildSize = initialRecvs[0];
        const int theirChildLowerStructSize = initialRecvs[1];
        // Perform the exchange
        sendBuffer.resize( myChildLowerStructSize );
        recvBuffer.resize( theirChildLowerStructSize );
        std::memcpy
        ( &sendBuffer[0], &factChildSN.lowerStruct[0], 
          myChildLowerStructSize*sizeof(int) );
        mpi::SendRecv
        ( &sendBuffer[0], myChildLowerStructSize, partner, 0,
          &recvBuffer[0], theirChildLowerStructSize, partner, 0, 
          distOrig.comm );
        
        // Union the two child lower structures
        childrenStruct.resize
        ( myChildLowerStructSize+theirChildLowerStructSize );
        it = std::set_union
        ( sendBuffer.begin(), sendBuffer.end(),
          recvBuffer.begin(), recvBuffer.end(), childrenStruct.begin() );
        const int childrenStructSize = int(it-childrenStruct.begin());
        childrenStruct.resize( childrenStructSize );

        // Union the lower structure of this supernode
        partialStruct.resize( childrenStructSize + origSN.lowerStruct.size() );
        it = std::set_union
        ( childrenStruct.begin(), childrenStruct.end(),
          origSN.lowerStruct.begin(), origSN.lowerStruct.end(),
          partialStruct.begin() );
        const int partialStructSize = int(it-partialStruct.begin());
        partialStruct.resize( partialStructSize );

        // Union again with the supernode indices
        supernodeIndices.resize( origSN.size );
        for( int i=0; i<origSN.size; ++i )
            supernodeIndices[i] = origSN.offset + i;
        fullStruct.resize( origSN.size + partialStructSize );
        it = std::set_union
        ( supernodeIndices.begin(), supernodeIndices.end(),
          partialStruct.begin(), partialStruct.end(), 
          fullStruct.begin() );
        const int fullStructSize = int(it-fullStruct.begin());
        fullStruct.resize( fullStructSize );

        // Construct the relative indices of the original lower structure
        const int numOrigLowerIndices = origSN.lowerStruct.size();
        it = fullStruct.begin();
        for( int i=0; i<numOrigLowerIndices; ++i )
        {
            const int index = origSN.lowerStruct[i];
            it = std::lower_bound( it, fullStruct.end(), index );
            factSN.origLowerRelIndices[index] = int(it-fullStruct.begin());
        }

        // Construct the relative indices of the children
        int numLeftIndices, numRightIndices;
        const int *leftIndices, *rightIndices;
        if( onLeft )
        {
            factSN.leftChildSize = myChildSize;
            factSN.rightChildSize = theirChildSize;
            leftIndices = &sendBuffer[0];
            rightIndices = &recvBuffer[0];
            numLeftIndices = sendBuffer.size();
            numRightIndices = recvBuffer.size();
        }
        else
        {
            factSN.leftChildSize = theirChildSize;
            factSN.rightChildSize = myChildSize;
            leftIndices = &recvBuffer[0];
            rightIndices = &sendBuffer[0];
            numLeftIndices = recvBuffer.size();
            numRightIndices = sendBuffer.size();
        }
        factSN.leftChildRelIndices.resize( numLeftIndices );
        it = fullStruct.begin();
        for( int i=0; i<numLeftIndices; ++i )
        {
            const int index = leftIndices[i];
            it = std::lower_bound( it, fullStruct.end(), index );
            factSN.leftChildRelIndices[i] = int(it-fullStruct.begin());
        }
        factSN.rightChildRelIndices.resize( numRightIndices );
        it = fullStruct.begin();
        for( int i=0; i<numRightIndices; ++i )
        {
            const int index = rightIndices[i];
            it = std::lower_bound( it, fullStruct.end(), index );
            factSN.rightChildRelIndices[i] = int(it-fullStruct.begin());
        }

        // Form lower structure of this node by removing the supernode indices
        const int lowerStructSize = fullStructSize - origSN.size;
        factSN.lowerStruct.resize( lowerStructSize );
        for( int i=0; i<lowerStructSize; ++i )
            factSN.lowerStruct[i] = fullStruct[origSN.size+i];

#ifndef RELEASE
        // Ensure that our partner computed a lowerStruct of the same size
        int partnerLowerStructSize;
        mpi::SendRecv
        ( &lowerStructSize,        1, partner, 0,
          &partnerLowerStructSize, 1, partner, 0, distOrig.comm );
        if( partnerLowerStructSize != lowerStructSize )
        {
            std::ostringstream msg;
            msg << "Partner's (" << partner << "'s) lower struct size was " 
                << partnerLowerStructSize << " for supernode " << s << "\n";
            throw std::logic_error( msg.str().c_str() );
        }
#endif

        // Fill numChildFactSendIndices so that we can reuse it for many facts.
        factSN.numChildFactSendIndices.resize( teamSize );
        std::memset
        ( &factSN.numChildFactSendIndices[0], 0, teamSize*sizeof(int) );
        const std::vector<int>& myChildRelIndices = 
            ( onLeft ? factSN.leftChildRelIndices 
                     : factSN.rightChildRelIndices );
        const int updateSize = myChildLowerStructSize;
        {
            const int updateColAlignment = myChildSize % childGridHeight;
            const int updateRowAlignment = myChildSize % childGridWidth;
            const int updateColShift = 
                Shift( childGridRow, updateColAlignment, childGridHeight );
            const int updateRowShift = 
                Shift( childGridCol, updateRowAlignment, childGridWidth );
            const int updateLocalHeight = 
                LocalLength( updateSize, updateColShift, childGridHeight );
            const int updateLocalWidth = 
                LocalLength( updateSize, updateRowShift, childGridWidth );
            for( int jChildLocal=0; 
                     jChildLocal<updateLocalWidth; ++jChildLocal )
            {
                const int jChild = updateRowShift + jChildLocal*childGridWidth;
                const int destGridCol = myChildRelIndices[jChild] % gridWidth;

                const int align = (jChild+updateRowAlignment) % childGridHeight;
                const int shift = 
                    (childGridRow+childGridHeight-align) % childGridHeight;
                const int localColShift = 
                    (jChild+shift-updateColShift) / childGridHeight;
                for( int iChildLocal=localColShift; 
                         iChildLocal<updateLocalHeight; ++iChildLocal )
                {
                    const int iChild = 
                        updateColShift + iChildLocal*childGridHeight;
                    const int destGridRow = 
                        myChildRelIndices[iChild] % gridHeight;

                    const int destRank = destGridRow + destGridCol*gridHeight;
                    ++factSN.numChildFactSendIndices[destRank];
                }
            }
        }

        // Fill numChildSolveSendIndices to use for many solves
        factSN.numChildSolveSendIndices.resize( teamSize );
        std::memset
        ( &factSN.numChildSolveSendIndices[0], 0, teamSize*sizeof(int) );
        {
            const int updateAlignment = myChildSize % childTeamSize;
            const int updateShift = 
                Shift( childTeamRank, updateAlignment, childTeamSize );
            const int updateLocalHeight = 
                LocalLength( updateSize, updateShift, childTeamSize );
            for( int iChildLocal=0; 
                     iChildLocal<updateLocalHeight; ++iChildLocal )
            {
                const int iChild = updateShift + iChildLocal*childTeamSize;
                const int destRank = myChildRelIndices[iChild] % teamSize;
                ++factSN.numChildSolveSendIndices[destRank];
            }
        }

        // Fill {left,right}ChildFact{Col,Row}Indices so that we can reuse them
        // to compute our recv information for use in many factorizations
        factSN.leftChildFactColIndices.clear();
        for( int i=0; i<numLeftIndices; ++i )
            if( factSN.leftChildRelIndices[i] % gridHeight == gridRow )
                factSN.leftChildFactColIndices.push_back( i );
        factSN.leftChildFactRowIndices.clear();
        for( int i=0; i<numLeftIndices; ++i )
            if( factSN.leftChildRelIndices[i] % gridWidth == gridCol )
                factSN.leftChildFactRowIndices.push_back( i );
        factSN.rightChildFactColIndices.clear();
        for( int i=0; i<numRightIndices; ++i )
            if( factSN.rightChildRelIndices[i] % gridHeight == gridRow )
                factSN.rightChildFactColIndices.push_back( i );
        factSN.rightChildFactRowIndices.clear();
        for( int i=0; i<numRightIndices; ++i )
            if( factSN.rightChildRelIndices[i] % gridWidth == gridCol )
                factSN.rightChildFactRowIndices.push_back( i );

        // Fill {left,right}ChildSolveIndices for use in many solves
        factSN.leftChildSolveIndices.clear();
        for( int i=0; i<numLeftIndices; ++i )
            if( factSN.leftChildRelIndices[i] % teamSize == teamRank )
                factSN.leftChildSolveIndices.push_back( i );
        factSN.rightChildSolveIndices.clear();
        for( int i=0; i<numRightIndices; ++i )
            if( factSN.rightChildRelIndices[i] % teamSize == teamRank )
                factSN.rightChildSolveIndices.push_back( i );

        //
        // Compute the solve recv indices
        //
        const int leftChildTeamSize = teamSize / 2;
        const int rightChildTeamSize = teamSize / 2;
        factSN.childSolveRecvIndices.clear();
        factSN.childSolveRecvIndices.resize( teamSize );

        // Compute the recv indices for the left child 
        const int leftUpdateAlignment = 
            factSN.leftChildSize % leftChildTeamSize;
        for( int iPre=0; iPre<factSN.leftChildSolveIndices.size(); ++iPre )
        {
            const int iChild = factSN.leftChildSolveIndices[iPre];
            const int iFront = factSN.leftChildRelIndices[iChild];
            const int iFrontLocal = (iFront-teamRank) / teamSize;

            const int childRank = 
                (iChild+leftUpdateAlignment) % leftChildTeamSize;
            const int frontRank = childRank;
            factSN.childSolveRecvIndices[frontRank].push_back(iFrontLocal);
        }

        // Compute the recv indices for the right child
        const int rightUpdateAlignment = 
            factSN.rightChildSize % rightChildTeamSize;
        for( int iPre=0; iPre<factSN.rightChildSolveIndices.size(); ++iPre )
        {
            const int iChild = factSN.rightChildSolveIndices[iPre];
            const int iFront = factSN.rightChildRelIndices[iChild];
            const int iFrontLocal = (iFront-teamRank) / teamSize;

            const int childRank = 
                (iChild+rightUpdateAlignment) % rightChildTeamSize;
            const int frontRank = leftChildTeamSize + childRank;
            factSN.childSolveRecvIndices[frontRank].push_back(iFrontLocal);
        }

        // Optionally compute the recv indices for the factorization. 
        // This is optional since it requires a nontrivial amount of storage.
        if( storeFactRecvIndices )
            ComputeFactRecvIndices( factSN, factChildSN );
        else
            factSN.childFactRecvIndices.clear();

        myOffset += factSN.size;
        localOffset1d += factSN.localSize1d;
    }
#ifndef RELEASE
    PopCallStack();
#endif
}

void clique::symbolic::ComputeFactRecvIndices
( const DistSymmFactSupernode& sn, 
  const DistSymmFactSupernode& childSN )
{
#ifndef RELEASE
    PushCallStack("symbolic::ComputeFactRecvIndices");
#endif
    const int commRank = mpi::CommRank( sn.comm );
    const int commSize = mpi::CommSize( sn.comm );
    const int gridHeight = sn.grid->Height();
    const int gridWidth = sn.grid->Width();
    const int gridRow = sn.grid->MCRank();
    const int gridCol = sn.grid->MRRank();
    const int childGridHeight = childSN.grid->Height();
    const int childGridWidth = childSN.grid->Width();

    // Assuming that we have a power of two number of processes, the following
    // should be valid. It will eventually need to be improved.
    const int rightChildOffset = commSize / 2;
    const int leftChildGridHeight = childGridHeight;
    const int leftChildGridWidth = childGridWidth;
    const int rightChildGridHeight = childGridHeight;
    const int rightChildGridWidth = childGridWidth;

    sn.childFactRecvIndices.clear();
    sn.childFactRecvIndices.resize( commSize );
    std::deque<int>::const_iterator it;

    // Compute the recv indices of the left child from each process 
    const int leftUpdateColAlignment = sn.leftChildSize % leftChildGridHeight;
    const int leftUpdateRowAlignment = sn.leftChildSize % leftChildGridWidth;
    for( int jPre=0; jPre<sn.leftChildFactRowIndices.size(); ++jPre )
    {
        const int jChild = sn.leftChildFactRowIndices[jPre];
        const int jFront = sn.leftChildRelIndices[jChild];
        const int jFrontLocal = (jFront-gridCol) / gridWidth;

        const int childCol = 
            (jChild+leftUpdateRowAlignment) % leftChildGridWidth;

        // Find the first iPre that maps to the lower triangle
        it = std::lower_bound
             ( sn.leftChildFactColIndices.begin(),
               sn.leftChildFactColIndices.end(), jChild );
        const int iPreStart = int(it-sn.leftChildFactColIndices.begin());
        for( int iPre=iPreStart; 
                 iPre<sn.leftChildFactColIndices.size(); ++iPre )
        {
            const int iChild = sn.leftChildFactColIndices[iPre];
            const int iFront = sn.leftChildRelIndices[iChild];
            const int iFrontLocal = (iFront-gridRow) / gridHeight;

            const int childRow = 
                (iChild+leftUpdateColAlignment) % leftChildGridHeight;
            const int childRank = childRow + childCol*leftChildGridHeight;

            const int frontRank = childRank;
            sn.childFactRecvIndices[frontRank].push_back(iFrontLocal);
            sn.childFactRecvIndices[frontRank].push_back(jFrontLocal);
        }
    }
    
    // Compute the recv indices of the right child from each process 
    const int rightUpdateColAlignment = 
        sn.rightChildSize % rightChildGridHeight;
    const int rightUpdateRowAlignment = sn.rightChildSize % rightChildGridWidth;
    for( int jPre=0; jPre<sn.rightChildFactRowIndices.size(); ++jPre )
    {
        const int jChild = sn.rightChildFactRowIndices[jPre];
        const int jFront = sn.rightChildRelIndices[jChild];
        const int jFrontLocal = (jFront-gridCol) / gridWidth;

        const int childCol = 
            (jChild+rightUpdateRowAlignment) % rightChildGridWidth;

        // Find the first iPre that maps to the lower triangle
        it = std::lower_bound
             ( sn.rightChildFactColIndices.begin(),
               sn.rightChildFactColIndices.end(), jChild );
        const int iPreStart = int(it-sn.rightChildFactColIndices.begin());
        for( int iPre=iPreStart; 
                 iPre<sn.rightChildFactColIndices.size(); ++iPre )
        {
            const int iChild = sn.rightChildFactColIndices[iPre];
            const int iFront = sn.rightChildRelIndices[iChild];
            const int iFrontLocal = (iFront-gridRow) / gridHeight;

            const int childRow = 
                (iChild+rightUpdateColAlignment) % rightChildGridHeight;
            const int childRank = childRow + childCol*rightChildGridHeight;

            const int frontRank = rightChildOffset + childRank;
            sn.childFactRecvIndices[frontRank].push_back(iFrontLocal);
            sn.childFactRecvIndices[frontRank].push_back(jFrontLocal);
        }
    }
#ifndef RELEASE
    PopCallStack();
#endif
}

