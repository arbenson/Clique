/*
   Copyright (c) 2009-2013, Jack Poulson, Lexing Ying,
   The University of Texas at Austin, and Stanford University
   All rights reserved.
 
   This file is part of Clique and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/

namespace cliq {

template<typename T>
class Vector
{
public:
    // Constructors and destructors
    Vector();
    Vector( int height );
    // TODO: Constructor for building from a Vector
    ~Vector();

    // High-level information
    int Height() const;

    // Data
    T Get( int row ) const;
    void Set( int row, T value );
    void Update( int row, T value );

    // For modifying the size of the vector
    void Empty();
    void ResizeTo( int height );

    // Assignment
    const Vector<T>& operator=( const Vector<T>& x );

private:
    Matrix<T> vec_;
};

// Set all of the entries of x to zero
template<typename T>
void MakeZeros( Vector<T>& x );

// Draw the entries of x uniformly from the unitball in T
template<typename T>
void MakeUniform( Vector<T>& x );

// Just an l2 norm for now
template<typename F>
BASE(F) Norm( const Vector<F>& x );

// y := alpha x + y
template<typename T>
void Axpy( T alpha, const Vector<T>& x, Vector<T>& y );

} // namespace cliq