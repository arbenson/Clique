/*
   Copyright (c) 2009-2013, Jack Poulson, Lexing Ying,
   The University of Texas at Austin, and Stanford University
   All rights reserved.
 
   This file is part of Clique and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef CLIQ_NUMERIC_LDL_DISTFRONTBLOCK_HPP
#define CLIQ_NUMERIC_LDL_DISTFRONTBLOCK_HPP

namespace cliq {

template<typename F> 
void FrontBlockLDL
( DistMatrix<F>& AL, DistMatrix<F>& ABR, 
  bool conjugate=false, bool intraPiv=false );

//----------------------------------------------------------------------------//
// Implementation begins here                                                 //
//----------------------------------------------------------------------------//

template<typename F> 
inline void FrontBlockLDL
( DistMatrix<F>& AL, DistMatrix<F>& ABR, bool conjugate, bool intraPiv )
{
    DEBUG_ONLY(CallStackEntry cse("internal::FrontBlockLDL"))
    const Grid& g = AL.Grid();
    DistMatrix<F> ATL(g), ABL(g);
    PartitionDown( AL, ATL, ABL, AL.Width() );

    // Make a copy of the original contents of ABL
    DistMatrix<F> BBL( ABL );

    if( intraPiv )
    {
        DistMatrix<Int,VC,STAR> p( ATL.Grid() );
        DistMatrix<F,MD,STAR> dSub( ATL.Grid() );
        // TODO: Expose the pivot type as an option?
        elem::ldl::Pivoted( ATL, dSub, p, conjugate, elem::BUNCH_KAUFMAN_A );

        // Solve against ABL and update ABR
        elem::ldl::SolveAfter( ATL, dSub, p, ABL, conjugate );
        const Orientation orientation = ( conjugate ? ADJOINT : TRANSPOSE );
        elem::Gemm( NORMAL, orientation, F(-1), ABL, BBL, F(1), ABR );

        // Copy the original contents of ABL back
        ABL = BBL;

        // Finish inverting ATL
        elem::TriangularInverse( LOWER, UNIT, ATL );
        elem::Trdtrmm( LOWER, ATL, dSub, conjugate ); 
        elem::ApplyInverseSymmetricPivots( LOWER, ATL, p, conjugate );
    }
    else
    {
        // Call the standard routine
        FrontLDL( AL, ABR, conjugate );

        // Copy the original contents of ABL back
        ABL = BBL;

        // Finish inverting ATL
        elem::TriangularInverse( LOWER, UNIT, ATL );
        elem::Trdtrmm( LOWER, ATL, conjugate );
    }
    elem::MakeSymmetric( LOWER, ATL, conjugate );
}

} // namespace cliq

#endif // ifndef CLIQ_NUMERIC_LDL_DISTFRONTBLOCK_HPP
