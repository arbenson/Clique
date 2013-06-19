/*
   Copyright (c) 2009-2013, Jack Poulson, Lexing Ying,
   The University of Texas at Austin, and Stanford University
   All rights reserved.
 
   This file is part of Clique and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/

namespace cliq {

// Use a simple 1d distribution where each process owns a fixed number of rows,
//     if last process,  height - (commSize-1)*floor(height/commSize)
//     otherwise,        floor(height/commSize)
template<typename T>
class DistVector
{
public:
    // Constructors and destructors
    DistVector();
    DistVector( mpi::Comm comm );
    DistVector( int height, mpi::Comm comm );
    DistVector( int height, T* buffer, mpi::Comm comm );
    DistVector( int height, const T* buffer, mpi::Comm comm );
    // TODO: Constructor for building from a DistVector
    ~DistVector();

    // High-level information
    int Height() const;

    // Communicator management
    void SetComm( mpi::Comm comm );
    mpi::Comm Comm() const;

    // Distribution information
    int Blocksize() const;
    int FirstLocalRow() const;
    int LocalHeight() const;

    // Local data
    T GetLocal( int localRow ) const;
    void SetLocal( int localRow, T value );
    void UpdateLocal( int localRow, T value );
    Matrix<T>& Vector();
    const Matrix<T>& Vector() const;

    // For modifying the size of the vector
    void Empty();
    void ResizeTo( int height );

    // Assignment
    const DistVector<T>& operator=( const DistVector<T>& x );

private:
    int height_;

    mpi::Comm comm_;

    int blocksize_;
    int firstLocalRow_;

    Matrix<T> vec_;
};

// Set all of the entries of x to zero
template<typename T>
void MakeZeros( DistVector<T>& x );

// Draw the entries of x uniformly from the unitball in T
template<typename T>
void MakeUniform( DistVector<T>& x );

// Just an l2 norm for now
template<typename F>
BASE(F) Norm( const DistVector<F>& x );

// y := alpha x + y
template<typename T>
void Axpy( T alpha, const DistVector<T>& x, DistVector<T>& y );

} // namespace cliq