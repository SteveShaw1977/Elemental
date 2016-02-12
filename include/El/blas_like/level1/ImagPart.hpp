/*
   Copyright (c) 2009-2016, Jack Poulson
   All rights reserved.

   This file is part of Elemental and is under the BSD 2-Clause License, 
   which can be found in the LICENSE file in the root directory, or at 
   http://opensource.org/licenses/BSD-2-Clause
*/
#include "El.hpp"

namespace El {

template<typename T>
void ImagPart( const Matrix<T>& A, Matrix<Base<T>>& AImag )
{
    DEBUG_ONLY(CSE cse("EntrywiseMap"))
    const Int m = A.Height();
    const Int n = A.Width();
    AImag.Resize( m, n );
    for( Int j=0; j<n; ++j )
        for( Int i=0; i<m; ++i )
            AImag.Set( i, j, A.GetImagPart(i,j) );
}

template<typename T>
void ImagPart
( const ElementalMatrix<T>& A, ElementalMatrix<Base<T>>& AImag )
{ 
    auto imagPart = []( T alpha ) { return ImagPart(alpha); };
    function<Base<T>(T)> realLambda( imagPart );
    EntrywiseMap( A, AImag, realLambda );
}

template<typename T>
void ImagPart
( const BlockMatrix<T>& A, BlockMatrix<Base<T>>& AImag )
{ 
    auto imagPart = []( T alpha ) { return ImagPart(alpha); };
    function<Base<T>(T)> realLambda( imagPart );
    EntrywiseMap( A, AImag, realLambda );
}

#define PROTO(T) \
  template void ImagPart( const Matrix<T>& A, Matrix<Base<T>>& AImag ); \
  template void ImagPart \
  ( const ElementalMatrix<T>& A, ElementalMatrix<Base<T>>& AImag ); \
  template void ImagPart \
  ( const BlockMatrix<T>& A, \
          BlockMatrix<Base<T>>& AImag );

#define EL_ENABLE_DOUBLEDOUBLE
#define EL_ENABLE_QUADDOUBLE
#define EL_ENABLE_QUAD
#define EL_ENABLE_BIGINT
#define EL_ENABLE_BIGFLOAT
#include "El/macros/Instantiate.h"

} // namespace El