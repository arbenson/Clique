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
#ifndef CLIQUE_DIST_DENSE_SYMM_MATRIX_HPP
#define CLIQUE_DIST_DENSE_SYMM_MATRIX_HPP 1

#include "mpi.h"

namespace clique {

// A simple symmetric 2d block-cyclic dense distributed matrix. Since it is for 
// internal usage only, we can require that the upper-left block is full and 
// owned by the top-left process in the grid. We can also restrict access to 
// blocks and column panels of the lower triangle in order to facilitate packed 
// storage (which will be implemented later).
template<typename F>
class DistDenseSymmMatrix
{
private:
    int height_;
    int blockSize_;

    MPI_Comm comm_, cartComm_, colComm_, rowComm_;
    int gridHeight_, gridWidth_;
    int gridRow_, gridCol_;

    std::vector<F> buffer_;
    std::vector<F*> blockColBuffers_;
    std::vector<int> blockColHeights_, 
                     blockColWidths_,
                     blockColRowOffsets_,
                     blockColColOffsets_;

    static void BlockChol( int n, F* A, int lda );
    static void BlockLDL( bool conjugate, int n, F* A, int lda );
    void LDL( bool conjugate );

public:
    DistDenseSymmMatrix( MPI_Comm comm, int gridHeight, int gridWidth );
    DistDenseSymmMatrix
    ( int height, int blockSize, MPI_Comm comm, int gridHeight, int gridWidth );
    ~DistDenseSymmMatrix();

    void Reconfigure( int height, int blockSize );

    int Height() const;
    int BlockSize() const;
    int GridHeight() const;
    int GridWidth() const;
    int GridRow() const;
    int GridCol() const;
    MPI_Comm Comm() const;
    MPI_Comm CartComm() const;
    MPI_Comm ColComm() const;
    MPI_Comm RowComm() const;

    F* BlockColBuffer( int jLocalBlock );
    const F* BlockColBuffer( int jLocalBlock ) const;

    int BlockColHeight( int jLocalBlock ) const;
    int BlockColWidth( int jLocalBlock ) const;

    void Print( std::string s="" ) const;
    void MakeZero();
    void MakeIdentity();

    //void Chol();
    void LDLT();
    void LDLH();
};

} // namespace clique

//----------------------------------------------------------------------------//
// Implementation begins here                                                 //
//----------------------------------------------------------------------------//

namespace clique {

template<typename T>
inline
DistDenseSymmMatrix<T>::~DistDenseSymmMatrix()
{ }

template<typename T>
inline int
DistDenseSymmMatrix<T>::Height() const
{ return height_; }

template<typename T>
inline int
DistDenseSymmMatrix<T>::BlockSize() const
{ return blockSize_; }

template<typename T>
inline int
DistDenseSymmMatrix<T>::GridHeight() const
{ return gridHeight_; }

template<typename T>
inline int
DistDenseSymmMatrix<T>::GridWidth() const
{ return gridWidth_; }

template<typename T>
inline int
DistDenseSymmMatrix<T>::GridRow() const
{ return gridRow_; }

template<typename T>
inline int
DistDenseSymmMatrix<T>::GridCol() const
{ return gridCol_; }

template<typename T>
inline MPI_Comm
DistDenseSymmMatrix<T>::Comm() const
{ return comm_; }

template<typename T>
inline MPI_Comm
DistDenseSymmMatrix<T>::CartComm() const
{ return cartComm_; }

template<typename T>
inline MPI_Comm
DistDenseSymmMatrix<T>::RowComm() const
{ return rowComm_; }

template<typename T>
inline MPI_Comm
DistDenseSymmMatrix<T>::ColComm() const
{ return colComm_; }

template<typename T>
inline T*
DistDenseSymmMatrix<T>::BlockColBuffer( int jLocalBlock )
{
#ifndef RELEASE
    PushCallStack("DistDenseSymmMatrix::BlockColBuffer");
    if( jLocalBlock < 0 )
        throw std::logic_error("jLocalBlock was less than 0");
    if( jLocalBlock > blockColBuffers_.size() )
        throw std::logic_error("jLocalBlock was too large");
#endif
    T* pointer = blockColBuffers_[jLocalBlock];
#ifndef RELEASE
    PopCallStack();
#endif
    return pointer;
}

template<typename T>
inline const T*
DistDenseSymmMatrix<T>::BlockColBuffer( int jLocalBlock ) const
{
#ifndef RELEASE
    PushCallStack("DistDenseSymmMatrix::BlockColBuffer");
    if( jLocalBlock < 0 )
        throw std::logic_error("jLocalBlock was less than 0");
    if( jLocalBlock > blockColBuffers_.size() )
        throw std::logic_error("jLocalBlock was too large");
#endif
    const T* pointer = blockColBuffers_[jLocalBlock];
#ifndef RELEASE
    PopCallStack();
#endif
    return pointer;
}

template<typename T>
inline int
DistDenseSymmMatrix<T>::BlockColHeight( int jLocalBlock ) const
{
#ifndef RELEASE
    PushCallStack("DistDenseSymmMatrix::BlockColHeight");
    if( jLocalBlock < 0 )
        throw std::logic_error("jLocalBlock was less than 0");
    if( jLocalBlock > blockColHeights_.size() )
        throw std::logic_error("jLocalBlock was too large");
#endif
    const int localHeight = blockColHeights_[jLocalBlock];
#ifndef RELEASE
    PopCallStack();
#endif
    return localHeight;
}

template<typename T>
inline int
DistDenseSymmMatrix<T>::BlockColWidth( int jLocalBlock ) const
{
#ifndef RELEASE
    PushCallStack("DistDenseSymmMatrix::BlockColWidth");
    if( jLocalBlock < 0 )
        throw std::logic_error("jLocalBlock was less than 0");
    if( jLocalBlock > blockColWidths_.size() )
        throw std::logic_error("jLocalBlock was too large");
#endif
    const int localWidth = blockColWidths_[jLocalBlock];
#ifndef RELEASE
    PopCallStack();
#endif
    return localWidth;
}

} // namespace clique

#endif /* CLIQUE_DIST_DENSE_SYMM_MATRIX_HPP */
