/*
   Copyright (c) 2009-2010, Jack Poulson
   All rights reserved.

   This file is part of Elemental.

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions are met:

    - Redistributions of source code must retain the above copyright notice,
      this list of conditions and the following disclaimer.

    - Redistributions in binary form must reproduce the above copyright notice,
      this list of conditions and the following disclaimer in the documentation
      and/or other materials provided with the distribution.

    - Neither the name of the owner nor the names of its contributors
      may be used to endorse or promote products derived from this software
      without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
   AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
   IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
   ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
   LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
   CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
   SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
   INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
   CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
   ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
   POSSIBILITY OF SUCH DAMAGE.
*/
#include "elemental/blas_internal.hpp"
#include "elemental/lapack_internal.hpp"
using namespace std;
using namespace elemental;
using namespace elemental::wrappers::mpi;

template<typename R>
void
elemental::lapack::internal::PanelTridiagL
( DistMatrix<R,MC,MR>& A,
  DistMatrix<R,MC,MR>& U )
{
#ifndef RELEASE
    PushCallStack("lapack::internal::PanelHessenbergL");
    if( A.GetGrid() != U.GetGrid() )
        throw logic_error
        ( "A and U must be distributed over the same grid." );
    if( A.Height() != A.Width() )
        throw logic_error( "A must be square." );
    if( A.Height() != U.Height() )
        throw logic_error( "A and U must be the same height." );
    if( U.Height() < U.Width() )
        throw logic_error( "U must be a column panel." );
    if( U.ColAlignment() != A.ColAlignment() || 
        U.RowAlignment() != A.RowAlignment() )
        throw logic_error( "U and A must be aligned." );
#endif
    const Grid& g = A.GetGrid();

    // Matrix views 
    DistMatrix<R,MC,MR> 
        ATL(g), ATR(g),  A00(g), a01(g),     A02(g),  ACol(g),  alpha21T(g),
        ABL(g), ABR(g),  a10(g), alpha11(g), a12(g),            a21B(g),
                         A20(g), a21(g),     A22(g);
    DistMatrix<R,MC,MR> 
        UTL(g), UTR(g),  U00(g), u01(g),       U02(g),  UCol(g),
        UBL(g), UBR(g),  u10(g), upsilon11(g), u12(g),
                         U20(g), u21(g),       U22(g);

    // Temporary distributions
    DistMatrix<R,MC,  Star> a21_MC_Star(g);
    DistMatrix<R,MR,  Star> a21_MR_Star(g);
    DistMatrix<R,Star,MR  > z10_Star_MR(g);
    DistMatrix<R,MC,  MR  > z21(g);
    DistMatrix<R,MC,  Star> z21_MC_Star(g);
    DistMatrix<R,MR,  Star> z21_MR_Star(g);
    DistMatrix<R,MR,  MC  > z21_MR_MC(g);

    // Push to the blocksize of 1, then pop at the end of the routine
    PushBlocksizeStack( 1 );

    PartitionDownLeftDiagonal
    ( A, ATL, ATR,
         ABL, ABR, 0 );
    PartitionDownLeftDiagonal
    ( U, UTL, UTR,
         UBL, UBR, 0 );
    while( UTL.Width() < U.Width() )
    {
        RepartitionDownDiagonal
        ( ATL, /**/ ATR,  A00, /**/ a01,     A02,
         /*************/ /**********************/
               /**/       a10, /**/ alpha11, a12, 
          ABL, /**/ ABR,  A20, /**/ a21,     A22 );
        
        RepartitionDownDiagonal
        ( UTL, /**/ UTR,  U00, /**/ u01,       U02,
         /*************/ /************************/
               /**/       u10, /**/ upsilon11, u12,
          UBL, /**/ UBR,  U20, /**/ u21,       U22 );

        ACol.View2x1
        ( alpha11,
          a21 );

        UCol.View2x1
        ( upsilon11,
          w21 );
            
        PartitionDown
        ( a21, alpha21T,
               a21B,     1 );

        a21_MC_Star.AlignWith( A22 );
        a21_MR_Star.AlignWith( A22 );
        z10_Star_MR.AlignWith( W20 );
        z21.AlignWith( w21 );
        z21_MC_Star.AlignWith( A22 );
        z21_MR_Star.AlignWith( A22 );
        z21_MR_MC.AlignColsWith( A22 );
        z21_MC_Star.ResizeTo( u21.Height(), 1 );
        z21_MR_Star.ResizeTo( u21.Height(), 1 );
        z10_Star_MR.ResizeTo( 1, u10.Width() );
        z21_MC_Star.SetToZero();
        z21_MR_Star.SetToZero();
        //--------------------------------------------------------------------//
        R tau = 0; // Initializing avoids false compiler warnings
        const bool thisIsMyColumn = ( g.MRRank() == a21.RowAlignment() );
        if( thisIsMyColumn )
            tau = lapack::internal::ColReflector( alpha21T, a21B );

        // HERE
            
        //--------------------------------------------------------------------//
        a21_MC_Star.FreeAlignments();
        a21_MR_Star.FreeAlignments();
        z10_Star_MR.FreeAlignments();
        z21.FreeAlignments();
        z21_MC_Star.FreeAlignments();
        z21_MR_Star.FreeAlignments();
        z21_MR_MC.FreeAlignments();

        SlidePartitionDownDiagonal
        ( ATL, /**/ ATR,  A00, a01,     /**/ A02,
               /**/       a10, alpha11, /**/ a12,
         /*************/ /**********************/
          ABL, /**/ ABR,  A20, a21,     /**/ A22 );

        SlidePartitionDownDiagonal
        ( UTL, /**/ UTR,  U00, u01,     /**/ U02,
               /**/       u10, omega11, /**/ u12,
         /*************/ /**********************/
          UBL, /**/ UBR,  U20, u21,     /**/ U22 );
    }
    PopBlocksizeStack();
#ifndef RELEASE
    PopCallStack();
#endif
}

#ifndef WITHOUT_COMPLEX
template<typename R>
void
elemental::lapack::internal::PanelTridiagL
( DistMatrix<complex<R>,MC,MR  >& A,
  DistMatrix<complex<R>,MC,MR  >& W,
  DistMatrix<R,MD,Star>& e,
  DistMatrix<complex<R>,MD,Star>& t )
{
#ifndef RELEASE
    PushCallStack("lapack::internal::PanelTridiagL");
    if( A.GetGrid() != W.GetGrid() || 
        W.GetGrid() != e.GetGrid() || 
        e.GetGrid() != t.GetGrid() )
        throw logic_error
        ( "A, d, e, and t must be distributed over the same grid." );
    if( A.Height() != A.Width() )
        throw logic_error( "A must be square." );
    if( A.Height() != W.Height() )
        throw logic_error( "A and W must be the same height." );
    if( W.Height() < W.Width() )
        throw logic_error( "W must be a column panel." );
    if( W.ColAlignment() != A.ColAlignment() || 
        W.RowAlignment() != A.RowAlignment() )
        throw logic_error( "W and A must be aligned." );
    if( e.Height() != W.Width() || e.Width() != 1 )
        throw logic_error
        ( "e must be a column vector of the same length as W's width." );
    if( !e.AlignedWithDiag( A, -1 ) )
        throw logic_error( "e is not aligned with A's subdiagonal." );
    if( t.Height() != W.Width() || t.Width() != 1 )
        throw logic_error
        ( "t must be a column vector of the same length as W's width." );
    if( !t.AlignedWithDiag( A, -1 ) )
        throw logic_error( "t is not aligned with A's subdiagonal." );
#endif
    typedef complex<R> C;

    const Grid& g = A.GetGrid();

    // Matrix views 
    DistMatrix<C,MC,MR> 
        ATL(g), ATR(g),  A00(g), a01(g),     A02(g),  ACol(g),  alpha21T(g),
        ABL(g), ABR(g),  a10(g), alpha11(g), a12(g),            a21B(g),
                         A20(g), a21(g),     A22(g);
    DistMatrix<C,MC,MR> 
        WTL(g), WTR(g),  W00(g), w01(g),     W02(g),  WCol(g),
        WBL(g), WBR(g),  w10(g), omega11(g), w12(g),
                         W20(g), w21(g),     W22(g);
    DistMatrix<R,MD,Star> 
        eT(g),  e0(g),
        eB(g),  epsilon1(g),
                e2(g);
    DistMatrix<C,MD,Star>
        tT(g),  t0(g),
        tB(g),  tau1(g),
                t2(g);

    // Temporary distributions
    DistMatrix<C,MC,  MR  > a10Conj(g);
    DistMatrix<C,MC,  MR  > w10Conj(g);
    DistMatrix<C,MC,  Star> a21_MC_Star(g);
    DistMatrix<C,MR,  Star> a21_MR_Star(g);
    DistMatrix<C,Star,MR  > z10_Star_MR(g);
    DistMatrix<C,MC,  MR  > z21(g);
    DistMatrix<C,MC,  Star> z21_MC_Star(g);
    DistMatrix<C,MR,  Star> z21_MR_Star(g);
    DistMatrix<C,MR,  MC  > z21_MR_MC(g);

    // Push to the blocksize of 1, then pop at the end of the routine
    PushBlocksizeStack( 1 );

    PartitionDownLeftDiagonal
    ( A, ATL, ATR,
         ABL, ABR, 0 );
    PartitionDownLeftDiagonal
    ( W, WTL, WTR,
         WBL, WBR, 0 );
    PartitionDown
    ( e,  eT,
          eB, 0 );
    PartitionDown
    ( t,  tT,
          tB, 0 );
    while( WTL.Width() < W.Width() )
    {
        RepartitionDownDiagonal
        ( ATL, /**/ ATR,  A00, /**/ a01,     A02,
         /*************/ /**********************/
               /**/       a10, /**/ alpha11, a12, 
          ABL, /**/ ABR,  A20, /**/ a21,     A22 );
        
        RepartitionDownDiagonal
        ( WTL, /**/ WTR,  W00, /**/ w01,     W02,
         /*************/ /**********************/
               /**/       w10, /**/ omega11, w12,
          WBL, /**/ WBR,  W20, /**/ w21,     W22 );

        RepartitionDown
        ( eT,  e0,
         /**/ /********/
               epsilon1,
          eB,  e2 );

        RepartitionDown
        ( tT,  t0,
         /**/ /****/
               tau1,
          tB,  t2 );

        ACol.View2x1
        ( alpha11,
          a21 );

        WCol.View2x1
        ( omega11,
          w21 );
        
        PartitionDown
        ( a21, alpha21T,
               a21B,     1 );

        a21_MC_Star.AlignWith( A22 );
        a21_MR_Star.AlignWith( A22 );
        z10_Star_MR.AlignWith( W20 );
        z21.AlignWith( w21 );
        z21_MC_Star.AlignWith( A22 );
        z21_MR_Star.AlignWith( A22 );
        z21_MR_MC.AlignColsWith( A22 );
        z21_MC_Star.ResizeTo( w21.Height(), 1 );
        z21_MR_Star.ResizeTo( w21.Height(), 1 );
        z10_Star_MR.ResizeTo( 1, w10.Width() );
        z21_MC_Star.SetToZero();
        z21_MR_Star.SetToZero();
        //--------------------------------------------------------------------//
        alpha11.SetImag( 0, 0, (R)0 ); 
        blas::Conj( w10, w10Conj );
        blas::Gemv( Normal, (C)-1, ABL, w10Conj, (C)1, ACol );
        blas::Conj( a10, a10Conj );
        blas::Gemv( Normal, (C)-1, WBL, a10Conj, (C)1, ACol );
        alpha11.SetImag( 0, 0, (R)0 );

        C tau = 0; // Initializing avoids false compiler warnings
        const bool thisIsMyColumn = ( g.MRRank() == a21.RowAlignment() );
        if( thisIsMyColumn )
        {
            tau = lapack::internal::ColReflector( alpha21T, a21B );
            const bool thisIsMyRow = ( g.MCRank() == alpha21T.ColAlignment() );
            if( thisIsMyRow )
                tau1.LocalEntry(0,0) = tau;
        }
            
        alpha21T.GetRealDiagonal( epsilon1 );
        alpha21T.Set( 0, 0, (C)1 );

        a21_MR_Star = a21_MC_Star = a21;

        PopBlocksizeStack();
        blas::internal::LocalHemvColAccumulateL
        ( (C)1, A22, a21_MC_Star, a21_MR_Star, z21_MC_Star, z21_MR_Star );
        PushBlocksizeStack( 1 );

        blas::Gemv
        ( ConjugateTranspose, 
          (C)1, W20.LockedLocalMatrix(),
                a21_MC_Star.LockedLocalMatrix(),
          (C)0, z10_Star_MR.LocalMatrix() );
        z10_Star_MR.SumOverCol();

        blas::Gemv
        ( Normal, 
          (C)-1, A20.LockedLocalMatrix(),
                 z10_Star_MR.LockedLocalMatrix(),
          (C)+1, z21_MC_Star.LocalMatrix() );

        blas::Gemv
        ( ConjugateTranspose, 
          (C)1, A20.LockedLocalMatrix(),
                a21_MC_Star.LockedLocalMatrix(),
          (C)0, z10_Star_MR.LocalMatrix() );
        z10_Star_MR.SumOverCol();

        blas::Gemv
        ( Normal, 
          (C)-1, W20.LockedLocalMatrix(),
                 z10_Star_MR.LockedLocalMatrix(),
          (C)+1, z21_MC_Star.LocalMatrix() );

        w21.SumScatterFrom( z21_MC_Star );
        z21_MR_MC.SumScatterFrom( z21_MR_Star );
        z21 = z21_MR_MC;

        if( thisIsMyColumn )
        {
            blas::Axpy( (C)1, z21, w21 );
            blas::Scal( tau, w21 );

            C alpha;
            C myAlpha = -static_cast<C>(0.5)*tau*
                        blas::Dot( w21.LockedLocalMatrix(), 
                                   a21.LockedLocalMatrix() );            
            AllReduce( &myAlpha, &alpha, 1, MPI_SUM, g.MCComm() );
            blas::Axpy( alpha, a21, w21 );
        }
        //--------------------------------------------------------------------//
        a21_MC_Star.FreeAlignments();
        a21_MR_Star.FreeAlignments();
        z10_Star_MR.FreeAlignments();
        z21.FreeAlignments();
        z21_MC_Star.FreeAlignments();
        z21_MR_Star.FreeAlignments();
        z21_MR_MC.FreeAlignments();

        SlidePartitionDownDiagonal
        ( ATL, /**/ ATR,  A00, a01,     /**/ A02,
               /**/       a10, alpha11, /**/ a12,
         /*************/ /**********************/
          ABL, /**/ ABR,  A20, a21,     /**/ A22 );

        SlidePartitionDownDiagonal
        ( WTL, /**/ WTR,  W00, w01,     /**/ W02,
               /**/       w10, omega11, /**/ w12,
         /*************/ /**********************/
          WBL, /**/ WBR,  W20, w21,     /**/ W22 );
        
        SlidePartitionDown
        ( eT,  e0,
               epsilon1,
         /**/ /********/
          eB,  e2 );

        SlidePartitionDown
        ( tT,  t0,
         /**/ /****/
               tau1,
          tB,  t2 );
    }
    PopBlocksizeStack();
#ifndef RELEASE
    PopCallStack();
#endif
}
#endif // WITHOUT_COMPLEX

template void elemental::lapack::internal::PanelTridiagL
( DistMatrix<float,MC,MR  >& A,
  DistMatrix<float,MC,MR  >& W,
  DistMatrix<float,MD,Star>& e );

template void elemental::lapack::internal::PanelTridiagL
( DistMatrix<double,MC,MR  >& A,
  DistMatrix<double,MC,MR  >& W,
  DistMatrix<double,MD,Star>& e );

#ifndef WITHOUT_COMPLEX
template void elemental::lapack::internal::PanelTridiagL
( DistMatrix<scomplex,MC,MR  >& A,
  DistMatrix<scomplex,MC,MR  >& W,
  DistMatrix<float,   MD,Star>& e,
  DistMatrix<scomplex,MD,Star>& t );

template void elemental::lapack::internal::PanelTridiagL
( DistMatrix<dcomplex,MC,MR  >& A,
  DistMatrix<dcomplex,MC,MR  >& W,
  DistMatrix<double,  MD,Star>& e,
  DistMatrix<dcomplex,MD,Star>& t );
#endif

