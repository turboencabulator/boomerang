### simplified mnemonics

constructors

## Section F.2

   subi   D,A,v   is addi(   D, A, -v )
   subis  D,A,v   is addis(  D, A, -v )
   subic  D,A,v   is addic(  D, A, -v )
   subicq D,A,v   is addicq( D, A, -v )

   sub    D,A,B   is subf(   D, B, A )
   subq   D,A,B   is subfq(  D, B, A )
   subo   D,A,B   is subfo(  D, B, A )
   suboq  D,A,B   is subfoq( D, B, A )

   subc   D,A,B   is subfc(   D, B, A )
   subcq  D,A,B   is subfcq(  D, B, A )
   subco  D,A,B   is subfco(  D, B, A )
   subcoq D,A,B   is subfcoq( D, B, A )

## Section F.3

# Table F-2

   cmpdi  crfD, A, SIMM! is cmpi(  crfD, 1, A, SIMM! )
   cmpd   crfD, A, B     is cmp(   crfD, 1, A, B )
   cmpldi crfD, A, UIMM  is cmpli( crfD, 1, A, UIMM )
   cmpld  crfD, A, B     is cmpl(  crfD, 1, A, B )

# Table F-3

   cmpwi  crfD, A, SIMM! is cmpi(  crfD, 0, A, SIMM! )
   cmpw   crfD, A, B     is cmp(   crfD, 0, A, B )
   cmplwi crfD, A, UIMM  is cmpli( crfD, 0, A, UIMM )
   cmplw  crfD, A, B     is cmpl(  crfD, 0, A, B )

## Section F.4

# Table F-4 skipped, 64 bit ops

# Table F-5

   extlwi   A,S,n,p  is rlwinm( A,S,p,     0,   n- 1   )
   extrwi   A,S,n,p  is rlwinm( A,S,p+n,   32-n,31    )
   inslwi   A,S,n,p  is rlwimi( A,S,32-p,  p,   p+n- 1 )
   insrwi   A,S,n,p  is rlwimi( A,S,32-p-n,p,   p+n- 1 )
   rotlwi   A,S,n    is rlwinm( A,S,n,     0,   31    )
   rotrwi   A,S,n    is rlwinm( A,S,32-n,  0,   31    )
   rotlw    A,S,B    is rlwnm(  A,S,B,     0,   31    )
   slwi     A,S,n    is rlwinm( A,S,n,     0,   31-n  )
   srwi     A,S,n    is rlwinm( A,S,32-n,  n,   31    )
   clrlwi   A,S,n    is rlwinm( A,S,0,     n,   31    )
   clrrwi   A,S,n    is rlwinm( A,S,0,     0,   31-n  )
   clrlslwi A,S,p,n  is rlwinm( A,S,n,     p-n, 31-n  )

## Section F.6

# Table F-17

   crset  bx      is creqv( bx, bx, bx )
   crclr  bx      is crxor( bx, bx, bx )
   crmove bx, by  is cror(  bx, by, by )
   crnot  bx, by  is crnor( bx, by, by )

## Section F.8

# Table F-21

   mtxer     S    is mtspr( 1,       S )
   mtlr      S    is mtspr( 8,       S )
   mtctr     S    is mtspr( 9,       S )
   mtdsisr   S    is mtspr( 18,      S )
   mtdar     S    is mtspr( 19,      S )
   mtdec     S    is mtspr( 22,      S )
   mtsdr1    S    is mtspr( 25,      S )
   mtsrr0    S    is mtspr( 26,      S )
   mtsrr1    S    is mtspr( 27,      S )
   mtsprg  n,S    is mtspr( 272+n,   S )
   mtasr     S    is mtspr( 280,     S )
   mtear     S    is mtspr( 282,     S )
   mttbl     S    is mtspr( 284,     S )
   mttbu     S    is mtspr( 285,     S )
   mtibatu n,S    is mtspr( 528+2*n, S )
   mtibatl n,S    is mtspr( 529+2*n, S )
   mtdbatu n,S    is mtspr( 536+2*n, S )
   mtdbatl n,S    is mtspr( 537+2*n, S )

   mfxer   D      is mfspr( D, 1       )
   mflr    D      is mfspr( D, 8       )
   mfctr   D      is mfspr( D, 9       )
   mfdsisr D      is mfspr( D, 18      )
   mfdar   D      is mfspr( D, 19      )
   mfdec   D      is mfspr( D, 22      )
   mfsdr1  D      is mfspr( D, 25      )
   mfsrr0  D      is mfspr( D, 26      )
   mfsrr1  D      is mfspr( D, 27      )
   mfsprg  D,n    is mfspr( D, 272+n   )
   mfasr   D      is mfspr( D, 280     )
   mfear   D      is mfspr( D, 282     )
   mftbl   D      is mftb(  D, 268     )
   mftbu   D      is mftb(  D, 269     )
   mfpvr   D      is mfspr( D, 287     )
   mfibatu D,n    is mfspr( D, 528+2*n )
   mfibatl D,n    is mfspr( D, 529+2*n )
   mfdbatu D,n    is mfspr( D, 536+2*n )
   mfdbatl D,n    is mfspr( D, 537+2*n )

## Section F.9

   nop            is ori( 0, 0, 0 )
   li    D, v     is addi( D, 0, v )
   lis   D, v     is addis( D, 0, v )
   la    D, d!, A is addi( D, A, d )
   mr    A, S     is ori( A, S, 0 )  # PPC 604 has better performance with ori than or
   mrq   A, S     is orq( A, S, S )
   not   A, S     is nor( A, S, S )
   notq  A, S     is norq( A, S, S )
   mtcr  S        is mtcrf( 0xff, S )

# e additions
#
   lwi   D, x     is addis( D, 0, x@[16:31] ); ori(D, D, x@[0:15] )
