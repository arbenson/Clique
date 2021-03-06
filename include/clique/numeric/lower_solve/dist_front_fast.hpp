/*
   Copyright (c) 2009-2013, Jack Poulson, Lexing Ying,
   The University of Texas at Austin, and Stanford University
   All rights reserved.
 
   This file is part of Clique and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#pragma once
#ifndef CLIQ_NUMERIC_LOWERSOLVE_DISTFRONTFAST_HPP
#define CLIQ_NUMERIC_LOWERSOLVE_DISTFRONTFAST_HPP

namespace cliq {

template<typename F>
void FrontFastLowerForwardSolve
( const DistMatrix<F,VC,STAR>& L, DistMatrix<F,VC,STAR>& X );
template<typename F>
void FrontFastIntraPivLowerForwardSolve
( const DistMatrix<F,VC,STAR>& L, const DistMatrix<Int,VC,STAR>& p, 
  DistMatrix<F,VC,STAR>& X );

template<typename F>
void FrontFastLowerForwardSolve
( const DistMatrix<F>& L, DistMatrix<F,VC,STAR>& X );
template<typename F>
void FrontFastIntraPivLowerForwardSolve
( const DistMatrix<F>& L, const DistMatrix<Int,VC,STAR>& p, 
  DistMatrix<F,VC,STAR>& X );

template<typename F>
void FrontFastLowerForwardSolve( const DistMatrix<F>& L, DistMatrix<F>& X );
template<typename F>
void FrontFastIntraPivLowerForwardSolve
( const DistMatrix<F>& L, const DistMatrix<Int,VC,STAR>& p, DistMatrix<F>& X );

template<typename F>
void FrontFastLowerBackwardSolve
( const DistMatrix<F,VC,STAR>& L, DistMatrix<F,VC,STAR>& X, 
  bool conjugate=false );
template<typename F>
void FrontFastIntraPivLowerBackwardSolve
( const DistMatrix<F,VC,STAR>& L, const DistMatrix<Int,VC,STAR>& p, 
  DistMatrix<F,VC,STAR>& X, bool conjugate=false );

template<typename F>
void FrontFastLowerBackwardSolve
( const DistMatrix<F>& L, DistMatrix<F,VC,STAR>& X, bool conjugate=false );
template<typename F>
void FrontFastIntraPivLowerBackwardSolve
( const DistMatrix<F>& L, const DistMatrix<Int,VC,STAR>& p, 
  DistMatrix<F,VC,STAR>& X, bool conjugate=false );

template<typename F>
void FrontFastLowerBackwardSolve
( const DistMatrix<F>& L, DistMatrix<F>& X, bool conjugate=false );
template<typename F>
void FrontFastIntraPivLowerBackwardSolve
( const DistMatrix<F>& L, const DistMatrix<Int,VC,STAR>& p, 
  DistMatrix<F>& X, bool conjugate=false );

//----------------------------------------------------------------------------//
// Implementation begins here                                                 //
//----------------------------------------------------------------------------//

template<typename F>
inline void FrontFastLowerForwardSolve
( const DistMatrix<F,VC,STAR>& L, DistMatrix<F,VC,STAR>& X )
{
    DEBUG_ONLY(
        CallStackEntry cse("FrontFastLowerForwardSolve");
        if( L.Grid() != X.Grid() )
            LogicError("L and X must be distributed over the same grid");
        if( L.Height() < L.Width() || L.Height() != X.Height() )
        {
            std::ostringstream msg;
            msg << "Nonconformal solve:\n"
                << "  L ~ " << L.Height() << " x " << L.Width() << "\n"
                << "  X ~ " << X.Height() << " x " << X.Width() << "\n";
            LogicError( msg.str() );
        }
        if( L.ColAlign() != X.ColAlign() )
            LogicError("L and X are assumed to be aligned");
    )
    const Grid& g = L.Grid();
    if( g.Size() == 1 )
    {
        FrontLowerForwardSolve( L.LockedMatrix(), X.Matrix() );
        return;
    }

    // Separate the top and bottom portions of X and L
    const int snSize = L.Width();
    DistMatrix<F,VC,STAR> LT(g), LB(g), XT(g), XB(g);
    LockedPartitionDown( L, LT, LB, snSize );
    PartitionDown( X, XT, XB, snSize );

    // XT := LT XT
    DistMatrix<F,STAR,STAR> XT_STAR_STAR( XT );
    elem::LocalGemm( NORMAL, NORMAL, F(1), LT, XT_STAR_STAR, F(0), XT );

    // XB := XB - LB XT
    if( LB.Height() != 0 )
    {
        XT_STAR_STAR = XT;
        elem::LocalGemm( NORMAL, NORMAL, F(-1), LB, XT_STAR_STAR, F(1), XB );
    }
}

template<typename F>
inline void FrontFastIntraPivLowerForwardSolve
( const DistMatrix<F,VC,STAR>& L, const DistMatrix<Int,VC,STAR>& p, 
  DistMatrix<F,VC,STAR>& X )
{
    DEBUG_ONLY(CallStackEntry cse("FrontFastIntraPivLowerForwardSolve"))

    // TODO: Cache the send and recv data for the pivots to avoid p[*,*]
    const Grid& g = L.Grid();
    DistMatrix<F,VC,STAR> XT(g), XB(g);
    PartitionDown( X, XT, XB, L.Width() );
    elem::ApplyRowPivots( XT, p );

    FrontFastLowerForwardSolve( L, X );
}

template<typename F>
inline void FrontFastLowerForwardSolve
( const DistMatrix<F>& L, DistMatrix<F,VC,STAR>& X )
{
    DEBUG_ONLY(
        CallStackEntry cse("FrontFastLowerForwardSolve");
        if( L.Grid() != X.Grid() )
            LogicError("L and X must be distributed over the same grid");
        if( L.Height() < L.Width() || L.Height() != X.Height() )
        {
            std::ostringstream msg;
            msg << "Nonconformal solve:\n"
                << "  L ~ " << L.Height() << " x " << L.Width() << "\n"
                << "  X ~ " << X.Height() << " x " << X.Width() << "\n";
            LogicError( msg.str() );
        }
    )
    const Grid& g = L.Grid();
    if( g.Size() == 1 )
    {
        FrontLowerForwardSolve( L.LockedMatrix(), X.Matrix() );
        return;
    }

    // Separate the top and bottom portions of X and L
    const int snSize = L.Width();
    DistMatrix<F> LT(g), LB(g); 
    LockedPartitionDown( L, LT, LB, snSize );
    DistMatrix<F,VC,STAR> XT(g), XB(g);
    PartitionDown( X, XT, XB, snSize );

    // Get ready for the local multiply
    DistMatrix<F,MR,STAR> XT_MR_STAR(g);
    XT_MR_STAR.AlignWith( LT );

    {
        // ZT[MC,* ] := LT[MC,MR] XT[MR,* ], 
        DistMatrix<F,MC,STAR> ZT_MC_STAR(g);
        ZT_MC_STAR.AlignWith( LT );
        XT_MR_STAR = XT;
        elem::LocalGemm( NORMAL, NORMAL, F(1), LT, XT_MR_STAR, ZT_MC_STAR );

        // XT[VC,* ].SumScatterFrom( ZT[MC,* ] )
        XT.SumScatterFrom( ZT_MC_STAR );
    }

    if( LB.Height() != 0 )
    {
        // Set up for the local multiply
        XT_MR_STAR = XT;

        // ZB[MC,* ] := LB[MC,MR] XT[MR,* ]
        DistMatrix<F,MC,STAR> ZB_MC_STAR(g);
        ZB_MC_STAR.AlignWith( LB );
        elem::LocalGemm( NORMAL, NORMAL, F(-1), LB, XT_MR_STAR, ZB_MC_STAR );

        // XB[VC,* ] += ZB[MC,* ]
        XB.SumScatterUpdate( F(1), ZB_MC_STAR );
    }
}

template<typename F>
inline void FrontFastIntraPivLowerForwardSolve
( const DistMatrix<F>& L, const DistMatrix<Int,VC,STAR>& p,
  DistMatrix<F,VC,STAR>& X )
{
    DEBUG_ONLY(CallStackEntry cse("FrontFastIntraPivLowerForwardSolve"))

    // TODO: Cache the send and recv data for the pivots to avoid p[*,*]
    const Grid& g = L.Grid();
    DistMatrix<F,VC,STAR> XT(g), XB(g);
    PartitionDown( X, XT, XB, L.Width() );
    elem::ApplyRowPivots( XT, p );

    FrontFastLowerForwardSolve( L, X );
}

template<typename F>
inline void FrontFastLowerForwardSolve
( const DistMatrix<F>& L, DistMatrix<F>& X )
{
    DEBUG_ONLY(
        CallStackEntry cse("FrontFastLowerForwardSolve");
        if( L.Grid() != X.Grid() )
            LogicError("L and X must be distributed over the same grid");
        if( L.Height() < L.Width() || L.Height() != X.Height() )
        {
            std::ostringstream msg;
            msg << "Nonconformal solve:\n"
                << "  L ~ " << L.Height() << " x " << L.Width() << "\n"
                << "  X ~ " << X.Height() << " x " << X.Width() << "\n";
            LogicError( msg.str() );
        }
    )
    const Grid& g = L.Grid();
    if( g.Size() == 1 )
    {
        FrontLowerForwardSolve( L.LockedMatrix(), X.Matrix() );
        return;
    }

    // Separate the top and bottom portions of X and L
    const int snSize = L.Width();
    DistMatrix<F> LT(g), LB(g), XT(g), XB(g);
    LockedPartitionDown( L, LT, LB, snSize );
    PartitionDown( X, XT, XB, snSize );

    // XT := LT XT
    DistMatrix<F> YT( XT );
    elem::Gemm( NORMAL, NORMAL, F(1), LT, YT, F(0), XT );

    // XB := XB - LB XT
    elem::Gemm( NORMAL, NORMAL, F(-1), LB, XT, F(1), XB );
}

template<typename F>
inline void FrontFastIntraPivLowerForwardSolve
( const DistMatrix<F>& L, const DistMatrix<Int,VC,STAR>& p, DistMatrix<F>& X )
{
    DEBUG_ONLY(CallStackEntry cse("FrontFastIntraPivLowerForwardSolve"))

    // TODO: Cache the send and recv data for the pivots to avoid p[*,*]
    const Grid& g = L.Grid();
    DistMatrix<F> XT(g), XB(g);
    PartitionDown( X, XT, XB, L.Width() );
    elem::ApplyRowPivots( XT, p );

    FrontFastLowerForwardSolve( L, X );
}

template<typename F>
inline void FrontFastLowerBackwardSolve
( const DistMatrix<F,VC,STAR>& L, DistMatrix<F,VC,STAR>& X,
  bool conjugate )
{
    DEBUG_ONLY(
        CallStackEntry cse("FrontFastLowerBackwardSolve");
        if( L.Grid() != X.Grid() )
            LogicError("L and X must be distributed over the same grid");
        if( L.Height() < L.Width() || L.Height() != X.Height() )
        {
            std::ostringstream msg;
            msg << "Nonconformal solve:\n"
                << "  L ~ " << L.Height() << " x " << L.Width() << "\n"
                << "  X ~ " << X.Height() << " x " << X.Width() << "\n";
            LogicError( msg.str() );
        }
        if( L.ColAlign() != X.ColAlign() )
            LogicError("L and X are assumed to be aligned");
    )
    const Grid& g = L.Grid();
    if( g.Size() == 1 )
    {
        FrontLowerBackwardSolve( L.LockedMatrix(), X.Matrix(), conjugate );
        return;
    }

    const int snSize = L.Width();
    DistMatrix<F,VC,STAR> LT(g), LB(g), XT(g), XB(g);
    LockedPartitionDown( L, LT, LB, snSize );
    PartitionDown( X, XT, XB, snSize );

    // XT := XT - LB^{T/H} XB
    DistMatrix<F,STAR,STAR> Z(g);
    const Orientation orientation = ( conjugate ? ADJOINT : TRANSPOSE );
    if( XB.Height() != 0 )
    {
        elem::LocalGemm( orientation, NORMAL, F(-1), LB, XB, Z );
        XT.SumScatterUpdate( F(1), Z );
    }

    // XT := LT^{T/H} XT
    elem::LocalGemm( orientation, NORMAL, F(1), LT, XT, Z );
    XT.SumScatterFrom( Z );
}

template<typename F>
inline void FrontFastIntraPivLowerBackwardSolve
( const DistMatrix<F,VC,STAR>& L, const DistMatrix<Int,VC,STAR>& p,
  DistMatrix<F,VC,STAR>& X, bool conjugate )
{
    DEBUG_ONLY(CallStackEntry cse("FrontFastIntraPivLowerBackwardSolve"))

    FrontFastLowerBackwardSolve( L, X, conjugate );

    // TODO: Cache the send and recv data for the pivots to avoid p[*,*]
    const Grid& g = L.Grid();
    DistMatrix<F,VC,STAR> XT(g), XB(g);
    PartitionDown( X, XT, XB, L.Width() );
    elem::ApplyInverseRowPivots( XT, p );
}

template<typename F>
inline void FrontFastLowerBackwardSolve
( const DistMatrix<F>& L, DistMatrix<F,VC,STAR>& X, bool conjugate )
{
    DEBUG_ONLY(
        CallStackEntry cse("FrontFastLowerBackwardSolve");
        if( L.Grid() != X.Grid() )
            LogicError("L and X must be distributed over the same grid");
        if( L.Height() < L.Width() || L.Height() != X.Height() )
        {
            std::ostringstream msg;
            msg << "Nonconformal solve:\n"
                << "  L ~ " << L.Height() << " x " << L.Width() << "\n"
                << "  X ~ " << X.Height() << " x " << X.Width() << "\n";
            LogicError( msg.str() );
        }
    )
    const Grid& g = L.Grid();
    if( g.Size() == 1 )
    {
        FrontLowerBackwardSolve( L.LockedMatrix(), X.Matrix(), conjugate );
        return;
    }

    const int snSize = L.Width();
    DistMatrix<F> LT(g), LB(g); 
    LockedPartitionDown( L, LT, LB, snSize );
    DistMatrix<F,VC,STAR> XT(g), XB(g);
    PartitionDown( X, XT, XB, snSize );

    DistMatrix<F,MR,STAR> ZT_MR_STAR( g );
    DistMatrix<F,VR,STAR> ZT_VR_STAR( g );
    ZT_MR_STAR.AlignWith( LB );
    const Orientation orientation = ( conjugate ? ADJOINT : TRANSPOSE );
    if( XB.Height() != 0 )
    {
        // ZT[MR,* ] := -(LB[MC,MR])^{T/H} XB[MC,* ]
        DistMatrix<F,MC,STAR> XB_MC_STAR( g );
        XB_MC_STAR.AlignWith( LB );
        XB_MC_STAR = XB;
        elem::LocalGemm
        ( orientation, NORMAL, F(-1), LB, XB_MC_STAR, ZT_MR_STAR );

        // ZT[VR,* ].SumScatterFrom( ZT[MR,* ] )
        ZT_VR_STAR.SumScatterFrom( ZT_MR_STAR );

        // ZT[VC,* ] := ZT[VR,* ]
        DistMatrix<F,VC,STAR> ZT_VC_STAR( g );
        ZT_VC_STAR.AlignWith( XT );
        ZT_VC_STAR = ZT_VR_STAR;

        // XT[VC,* ] += ZT[VC,* ]
        elem::Axpy( F(1), ZT_VC_STAR, XT );
    }

    {
        // ZT[MR,* ] := (LT[MC,MR])^{T/H} XT[MC,* ]
        DistMatrix<F,MC,STAR> XT_MC_STAR( g );
        XT_MC_STAR.AlignWith( LT );
        XT_MC_STAR = XT;
        elem::LocalGemm
        ( orientation, NORMAL, F(1), LT, XT_MC_STAR, ZT_MR_STAR );

        // ZT[VR,* ].SumScatterFrom( ZT[MR,* ] )
        ZT_VR_STAR.SumScatterFrom( ZT_MR_STAR );

        // XT[VC,* ] := ZT[VR,* ]
        XT = ZT_VR_STAR;
    }
}

template<typename F>
inline void FrontFastIntraPivLowerBackwardSolve
( const DistMatrix<F>& L, const DistMatrix<Int,VC,STAR>& p,
  DistMatrix<F,VC,STAR>& X, bool conjugate )
{
    DEBUG_ONLY(CallStackEntry cse("FrontFastIntraPivLowerBackwardSolve"))

    FrontFastLowerBackwardSolve( L, X, conjugate );

    // TODO: Cache the send and recv data for the pivots to avoid p[*,*]
    const Grid& g = L.Grid();
    DistMatrix<F,VC,STAR> XT(g), XB(g);
    PartitionDown( X, XT, XB, L.Width() );
    elem::ApplyInverseRowPivots( XT, p );
}

template<typename F>
inline void FrontFastLowerBackwardSolve
( const DistMatrix<F>& L, DistMatrix<F>& X, bool conjugate )
{
    DEBUG_ONLY(
        CallStackEntry cse("FrontFastLowerBackwardSolve");
        if( L.Grid() != X.Grid() )
            LogicError("L and X must be distributed over the same grid");
        if( L.Height() < L.Width() || L.Height() != X.Height() )
        {
            std::ostringstream msg;
            msg << "Nonconformal solve:\n"
                << "  L ~ " << L.Height() << " x " << L.Width() << "\n"
                << "  X ~ " << X.Height() << " x " << X.Width() << "\n";
            LogicError( msg.str() );
        }
    )
    const Grid& g = L.Grid();
    if( g.Size() == 1 )
    {
        FrontLowerBackwardSolve( L.LockedMatrix(), X.Matrix(), conjugate );
        return;
    }

    const int snSize = L.Width();
    DistMatrix<F> LT(g), LB(g), XT(g), XB(g);
    LockedPartitionDown( L, LT, LB, snSize );
    PartitionDown( X, XT, XB, snSize );

    // XT := XT - LB^{T/H} XB
    const Orientation orientation = ( conjugate ? ADJOINT : TRANSPOSE );
    elem::Gemm( orientation, NORMAL, F(-1), LB, XB, F(1), XT );

    // XT := LT^{T/H} XT
    DistMatrix<F> Z(XT.Grid());
    elem::Gemm( orientation, NORMAL, F(1), LT, XT, Z );
    XT = Z;
}

template<typename F>
inline void FrontFastIntraPivLowerBackwardSolve
( const DistMatrix<F>& L, const DistMatrix<Int,VC,STAR>& p,
  DistMatrix<F>& X, bool conjugate )
{
    DEBUG_ONLY(CallStackEntry cse("FrontFastIntraPivLowerBackwardSolve"))

    FrontFastLowerBackwardSolve( L, X, conjugate );

    // TODO: Cache the send and recv data for the pivots to avoid p[*,*]
    const Grid& g = L.Grid();
    DistMatrix<F> XT(g), XB(g);
    PartitionDown( X, XT, XB, L.Width() );
    elem::ApplyInverseRowPivots( XT, p );
}

} // namespace cliq

#endif // ifndef CLIQ_NUMERIC_LOWERSOLVE_DISTFRONTFAST_HPP
