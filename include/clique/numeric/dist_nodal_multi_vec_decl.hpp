/*
   Copyright (c) 2009-2013, Jack Poulson, Lexing Ying,
   The University of Texas at Austin, and Stanford University
   All rights reserved.
 
   This file is part of Clique and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/

namespace cliq {

// For handling a set of vectors distributed in a [VC,* ] manner over each node
// of the elimination tree
template<typename F>
class DistNodalMultiVec
{
public:
    std::vector<Matrix<F> > localNodes;
    std::vector<DistMatrix<F,VC,STAR> > distNodes;

    DistNodalMultiVec();
    DistNodalMultiVec
    ( const DistMap& inverseMap, const DistSymmInfo& info,
      const DistMultiVec<F>& X );
    DistNodalMultiVec( const DistNodalMatrix<F>& X );

    const DistNodalMultiVec<F>& operator=( const DistNodalMatrix<F>& X );

    void Pull
    ( const DistMap& inverseMap, const DistSymmInfo& info,
      const DistMultiVec<F>& X );
    void Push
    ( const DistMap& inverseMap, const DistSymmInfo& info,
            DistMultiVec<F>& X ) const;

    int Height() const;
    int Width() const;

    int LocalHeight() const;
private:
    int height_, width_;
};

} // namespace cliq
