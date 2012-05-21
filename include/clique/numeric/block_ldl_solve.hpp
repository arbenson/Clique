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
#ifndef CLIQUE_NUMERIC_BLOCK_LDL_SOLVE_HPP
#define CLIQUE_NUMERIC_BLOCK_LDL_SOLVE_HPP 1

namespace cliq {
namespace numeric {

template<typename F>
void BlockLDLSolve
( Orientation orientation,
  const symbolic::SymmFact& S,
  const numeric::SymmFrontTree<F>& L,
        Matrix<F>& localX );

//----------------------------------------------------------------------------//
// Implementation begins here                                                 //
//----------------------------------------------------------------------------//

template<typename F>
inline void BlockLDLSolve
( Orientation orientation,
  const symbolic::SymmFact& S,
  const numeric::SymmFrontTree<F>& L,
        Matrix<F>& localX )
{
#ifndef RELEASE
    PushCallStack("numeric::BlockLDLSolve");
    if( orientation == NORMAL )
        throw std::logic_error("Invalid orientation for BlockLDL");
#endif
    // Solve against block diagonal factor, L D
    BlockLowerSolve( NORMAL, S, L, localX );

    // Solve against the (conjugate-)transpose of the block unit diagonal L
    BlockLowerSolve( orientation, S, L, localX );
#ifndef RELEASE
    PopCallStack();
#endif
}

} // namespace numeric
} // namespace cliq

#endif // CLIQUE_NUMERIC_BLOCK_LDL_SOLVE_HPP
