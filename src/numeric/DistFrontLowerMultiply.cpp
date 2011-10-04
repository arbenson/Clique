/*
   Clique: a scalable implementation of the multifrontal algorithm

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

template<typename F>
void clique::numeric::DistFrontLowerMultiplyNormal
( Diagonal diag, int diagOffset, int supernodeSize, 
  const DistMatrix<F,VC,STAR>& L, DistMatrix<F,VC,STAR>& X )
{
#ifndef RELEASE
    clique::PushCallStack("numeric::DistFrontLowerMultiplyNormal");
    if( L.Grid() != X.Grid() )
        throw std::logic_error
        ("L and X must be distributed over the same grid");
    if( L.Height() != L.Width() || L.Height() != X.Height() || 
        L.Height() < supernodeSize )
    {
        std::ostringstream msg;
        msg << "Nonconformal multiply:\n"
            << "  supernodeSize ~ " << supernodeSize << "\n"
            << "  L ~ " << L.Height() << " x " << L.Width() << "\n"
            << "  X ~ " << X.Height() << " x " << X.Width() << "\n";
        throw std::logic_error( msg.str().c_str() );
    }
    if( L.ColAlignment() != X.ColAlignment() )
        throw std::logic_error("L and X are assumed to be aligned");
#endif
    const Grid& g = L.Grid();

    // Matrix views
    DistMatrix<F,VC,STAR>
        LTL(g), LTR(g),  L00(g), L01(g), L02(g),
        LBL(g), LBR(g),  L10(g), L11(g), L12(g),
                         L20(g), L21(g), L22(g);

    DistMatrix<F,VC,STAR> XT(g),  X0(g),
                          XB(g),  X1(g),
                                  X2(g);

    // Temporary distributions
    DistMatrix<F,STAR,STAR> L11_STAR_STAR(g);
    DistMatrix<F,STAR,STAR> X1_STAR_STAR(g);

    // Start the algorithm
    LockedPartitionDownDiagonal
    ( L, LTL, LTR,
         LBL, LBR, 0 );
    PartitionDown
    ( X, XT,
         XB, 0 );
    while( XT.Height() < supernodeSize )
    {
        const int blocksize = std::min(Blocksize(),supernodeSize-XT.Height());
        LockedRepartitionDownDiagonal
        ( LTL, /**/ LTR,  L00, /**/ L01, L02,
         /*************/ /******************/
               /**/       L10, /**/ L11, L12,
          LBL, /**/ LBR,  L20, /**/ L21, L22, blocksize );

        RepartitionDown
        ( XT,  X0,
         /**/ /**/
               X1,
          XB,  X2, blocksize );

        //--------------------------------------------------------------------//
        // HERE
        throw std::logic_error("This routine is not yet finished");
        //--------------------------------------------------------------------//

        SlideLockedPartitionDownDiagonal
        ( LTL, /**/ LTR,  L00, L01, /**/ L02,
               /**/       L10, L11, /**/ L12,
         /*************/ /******************/
          LBL, /**/ LBR,  L20, L21, /**/ L22 );

        SlidePartitionDown
        ( XT,  X0,
               X1,
         /**/ /**/
          XB,  X2 );
    }
#ifndef RELEASE
    clique::PopCallStack();
#endif
}

template<typename F>
void clique::numeric::DistFrontLowerMultiplyTranspose
( Orientation orientation, Diagonal diag, int diagOffset, int supernodeSize,
  const DistMatrix<F,VC,STAR>& L, DistMatrix<F,VC,STAR>& X )
{
#ifndef RELEASE
    clique::PushCallStack("numeric::DistFrontLowerMultiplyTranspose");
    if( L.Grid() != X.Grid() )
        throw std::logic_error
        ("L and X must be distributed over the same grid");
    if( L.Height() != L.Width() || L.Height() != X.Height() || 
        L.Height() < supernodeSize )
    {
        std::ostringstream msg;
        msg << "Nonconformal multiply:\n"
            << "  supernodeSize ~ " << supernodeSize << "\n"
            << "  L ~ " << L.Height() << " x " << L.Width() << "\n"
            << "  X ~ " << X.Height() << " x " << X.Width() << "\n";
        throw std::logic_error( msg.str().c_str() );
    }
    if( L.ColAlignment() != X.ColAlignment() )
        throw std::logic_error("L and X are assumed to be aligned");
    if( orientation == NORMAL )
        throw std::logic_error("Orientation must be (conjugate-)transposed");
#endif
    const Grid& g = L.Grid();

    // Matrix views
    DistMatrix<F,VC,STAR>
        LTL(g), LTR(g),  L00(g), L01(g), L02(g),
        LBL(g), LBR(g),  L10(g), L11(g), L12(g),
                         L20(g), L21(g), L22(g);

    DistMatrix<F,VC,STAR> XT(g),  X0(g),
                          XB(g),  X1(g),
                                  X2(g);

    // Temporary distributions
    DistMatrix<F,STAR,STAR> L11_STAR_STAR(g);
    DistMatrix<F,STAR,STAR> X1_STAR_STAR(g);

    LockedPartitionUpDiagonal
    ( L, LTL, LTR,
         LBL, LBR, L.Height()-supernodeSize );
    PartitionUp
    ( X, XT,
         XB, X.Height()-supernodeSize );

    while( XT.Height() > 0 )
    {
        LockedRepartitionUpDiagonal
        ( LTL, /**/ LTR,   L00, L01, /**/ L02,
               /**/        L10, L11, /**/ L12,
         /*************/  /******************/
          LBL, /**/ LBR,   L20, L21, /**/ L22 );

        RepartitionUp
        ( XT,  X0,
               X1,
         /**/ /**/
          XB,  X2 );

        //--------------------------------------------------------------------//
        // HERE
        throw std::logic_error("This routine is not yet finished");
        //--------------------------------------------------------------------//

        SlideLockedPartitionUpDiagonal
        ( LTL, /**/ LTR,  L00, /**/ L01, L02,
         /*************/ /******************/
               /**/       L10, /**/ L11, L12,
          LBL, /**/ LBR,  L20, /**/ L21, L22 );

        SlidePartitionUp
        ( XT,  X0,
         /**/ /**/
               X1,
          XB,  X2 );
    }
#ifndef RELEASE
    clique::PopCallStack();
#endif
}

template void clique::numeric::DistFrontLowerMultiplyNormal
( Diagonal diag, int diagOffset, int supernodeSize,
  const DistMatrix<float,VC,STAR>& L,
        DistMatrix<float,VC,STAR>& X );
template void clique::numeric::DistFrontLowerMultiplyTranspose
( Orientation orientation, Diagonal diag, int diagOffset, int supernodeSize,
  const DistMatrix<float,VC,STAR>& L,
        DistMatrix<float,VC,STAR>& X );

template void clique::numeric::DistFrontLowerMultiplyNormal
( Diagonal diag, int diagOffset, int supernodeSize,
  const DistMatrix<double,VC,STAR>& L, 
        DistMatrix<double,VC,STAR>& X );
template void clique::numeric::DistFrontLowerMultiplyTranspose
( Orientation orientation, Diagonal diag, int diagOffset, int supernodeSize,
  const DistMatrix<double,VC,STAR>& L,
        DistMatrix<double,VC,STAR>& X );

template void clique::numeric::DistFrontLowerMultiplyNormal
( Diagonal diag, int diagOffset, int supernodeSize,
  const DistMatrix<std::complex<float>,VC,STAR>& L, 
        DistMatrix<std::complex<float>,VC,STAR>& X );
template void clique::numeric::DistFrontLowerMultiplyTranspose
( Orientation orientation, Diagonal diag, int diagOffset, int supernodeSize,
  const DistMatrix<std::complex<float>,VC,STAR>& L, 
        DistMatrix<std::complex<float>,VC,STAR>& X );

template void clique::numeric::DistFrontLowerMultiplyNormal
( Diagonal diag, int diagOffset, int supernodeSize,
  const DistMatrix<std::complex<double>,VC,STAR>& L, 
        DistMatrix<std::complex<double>,VC,STAR>& X );
template void clique::numeric::DistFrontLowerMultiplyTranspose
( Orientation orientation, Diagonal diag, int diagOffset, int supernodeSize,
  const DistMatrix<std::complex<double>,VC,STAR>& L,
        DistMatrix<std::complex<double>,VC,STAR>& X );
