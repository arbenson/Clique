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
class MultiVector
{
public:
    // Constructors and destructors
    MultiVector();
    MultiVector( int height, int width );
    // TODO: Constructor for building from a MultiVector
    ~MultiVector();

    // High-level information
    int Height() const;
    int Width() const;

    // Data
    T Get( int row, int col ) const;
    void Set( int row, int col, T value );
    void Update( int row, int col, T value );

    // For modifying the size of the multi-vector
    void Empty();
    void ResizeTo( int height, int width );

    // Assignment
    const MultiVector<T>& operator=( const Vector<T>& x );
    const MultiVector<T>& operator=( const MultiVector<T>& X );

private:
    Matrix<T> multiVec_;
};

// Set all of the entries of X to zero
template<typename T>
void MakeZeros( MultiVector<T>& X );

// Draw the entries of X uniformly from the unitball in T
template<typename T>
void MakeUniform( MultiVector<T>& X );

// Just an l2 norm for now
template<typename F>
void Norms( const MultiVector<F>& X, std::vector<BASE(F)>& norms );

// Y := alpha X + Y 
template<typename T>
void Axpy( T alpha, const MultiVector<T>& X, MultiVector<T>& Y );

} // namespace cliq