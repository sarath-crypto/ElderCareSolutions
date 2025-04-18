# KISS FFT [![Build Status](https://travis-ci.com/mborgerding/kissfft.svg?branch=master)](https://travis-ci.com/mborgerding/kissfft)

KISS FFT - A mixed-radix Fast Fourier Transform based up on the principle, 
"Keep It Simple, Stupid."

There are many great fft libraries already around.  Kiss FFT is not trying
to be better than any of them.  It only attempts to be a reasonably efficient, 
moderately useful FFT that can use fixed or floating data types and can be 
incorporated into someone's C program in a few minutes with trivial licensing.

## USAGE:

The basic usage for 1-d complex FFT is:

```c
    #include "kiss_fft.h"
    kiss_fft_cfg cfg = kiss_fft_alloc( nfft ,is_inverse_fft ,0,0 );
    while ...
    
        ... // put kth sample in cx_in[k].r and cx_in[k].i
        
        kiss_fft( cfg , cx_in , cx_out );
        
        ... // transformed. DC is in cx_out[0].r and cx_out[0].i 
        
    kiss_fft_free(cfg);
```
 - **Note**: frequency-domain data is stored from dc up to 2pi.
    so cx_out[0] is the dc bin of the FFT
    and cx_out[nfft/2] is the Nyquist bin (if exists)

Declarations are in "kiss_fft.h", along with a brief description of the 
functions you'll need to use. 

Code definitions for 1d complex FFTs are in kiss_fft.c.

You can do other cool stuff with the extras you'll find in tools/
> - multi-dimensional FFTs 
> - real-optimized FFTs  (returns the positive half-spectrum: 
    (nfft/2+1) complex frequency bins)
> - fast convolution FIR filtering (not available for fixed point)
> - spectrum image creation

The core fft and most tools/ code can be compiled to use float, double,
 Q15 short or Q31 samples. The default is float.

## BUILDING:

There are two functionally-equivalent build systems supported by kissfft:

 - Make (traditional Makefiles for Unix / Linux systems)
 - CMake (more modern and feature-rich build system developed by Kitware)

To build kissfft, the following build environment can be used:

 - GNU build environment with GCC, Clang and GNU Make or CMake (>= 3.6)
 - Microsoft Visual C++ (MSVC) with CMake (>= 3.6)

Additional libraries required to build and test kissfft include:

 - libpng for psdpng tool,
 - libfftw3 to validate kissfft results against it,
 - python 2/3 with Numpy to validate kissfft results against it.
 - OpenMP supported by GCC, Clang or MSVC for multi-core FFT transformations

Environments like Cygwin and MinGW can be highly likely used to build kissfft
targeting Windows platform, but no tests were performed to the date.

Both Make and CMake builds are easily configurable:

 - `KISSFFT_DATATYPE=<datatype>` (for Make) or `-DKISSFFT_DATATYPE=<datatype>`
   (for CMake) denote the principal datatype used by kissfft. It can be one
   of the following:

   - float (default)
   - double
   - int16_t
   - int32_t
   - SIMD (requires SSE instruction set support on target CPU)

 - `KISSFFT_OPENMP=1` (for Make) or `-DKISSFFT_OPENMP=ON` (for CMake) builds kissfft
   with OpenMP support. Please note that a supported compiler is required and this
   option is turned off by default.

 - `KISSFFT_STATIC=1` (for Make) or `-DKISSFFT_STATIC=ON` (for CMake) instructs
   the builder to create static library ('.lib' for Windows / '.a' for Unix or Linux).
   By default, this option is turned off and the shared library is created
   ('.dll' for Windows, '.so' for Linux or Unix, '.dylib' for Mac OSX)

 - `-DKISSFFT_TEST=OFF` (for CMake) disables building tests for kissfft. On Make,
   building tests is done separately by 'make testall' or 'make testsingle', so
   no specific setting is required.

 - `KISSFFT_TOOLS=0` (for Make) or `-DKISSFFT_TOOLS=OFF` (for CMake) builds kissfft
    without command-line tools like 'fastconv'. By default the tools are built.

    - `KISSFFT_USE_ALLOCA=1` (for Make) or `-DKISSFFT_USE_ALLOCA=ON` (for CMake)
       build kissfft with 'alloca' usage instead of 'malloc' / 'free'.

    - `PREFIX=/full/path/to/installation/prefix/directory` (for Make) or
      `-DCMAKE_INSTALL_PREFIX=/full/path/to/installation/prefix/directory` (for CMake)
      specifies the prefix directory to install kissfft into.

For example, to build kissfft as a static library with 'int16_t' datatype and
OpenMP support using Make, run the command from kissfft source tree:

```
make KISSFFT_DATATYPE=int16_t KISSFFT_STATIC=1 KISSFFT_OPENMP=1 all
```

The same configuration for CMake is:

```
mkdir build && cd build
cmake -DKISSFFT_DATATYPE=int16_t -DKISSFFT_STATIC=ON -DKISSFFT_OPENMP=ON ..
make all
```

To specify '/tmp/1234' as installation prefix directory, run:


```
make PREFIX=/tmp/1234 KISSFFT_DATATYPE=int16_t KISSFFT_STATIC=1 KISSFFT_OPENMP=1 install
```

or

```
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/tmp/1234 -DKISSFFT_DATATYPE=int16_t -DKISSFFT_STATIC=ON -DKISSFFT_OPENMP=ON ..
make all
make install
```

## TESTING:

To validate the build configured as an example above, run the following command from
kissfft source tree:

```
make KISSFFT_DATATYPE=int16_t KISSFFT_STATIC=1 KISSFFT_OPENMP=1 testsingle
```

if using Make, or:

```
make test
```

if using CMake.

To test all possible build configurations, please run an extended testsuite from
kissfft source tree:

```
sh test/kissfft-testsuite.sh
```

Please note that the extended testsuite takes around 20-40 minutes depending on device
it runs on. This testsuite is useful for reporting bugs or testing the pull requests.

## BACKGROUND

I started coding this because I couldn't find a fixed point FFT that didn't 
use assembly code.  I started with floating point numbers so I could get the 
theory straight before working on fixed point issues.  In the end, I had a 
little bit of code that could be recompiled easily to do ffts with short, float
or double (other types should be easy too).  

Once I got my FFT working, I was curious about the speed compared to
a well respected and highly optimized fft library.  I don't want to criticize 
this great library, so let's call it FFT_BRANDX.
During this process, I learned:

> 1. FFT_BRANDX has more than 100K lines of code. The core of kiss_fft is about 500 lines (cpx 1-d).
> 2. It took me an embarrassingly long time to get FFT_BRANDX working.
> 3. A simple program using FFT_BRANDX is 522KB. A similar program using kiss_fft is 18KB (without optimizing for size).
> 4. FFT_BRANDX is roughly twice as fast as KISS FFT in default mode.

It is wonderful that free, highly optimized libraries like FFT_BRANDX exist.
But such libraries carry a huge burden of complexity necessary to extract every 
last bit of performance.

**Sometimes simpler is better, even if it's not better.**

## FREQUENTLY ASKED QUESTIONS:
> Q: Can I use kissfft in a project with a ___ license?</br>
> A: Yes.  See LICENSE below.

> Q: Why don't I get the output I expect?</br>
> A: The two most common causes of this are
> 	1) scaling : is there a constant multiplier between what you got and what you want?
> 	2) mixed build environment -- all code must be compiled with same preprocessor 
> 	definitions for FIXED_POINT and kiss_fft_scalar

> Q: Will you write/debug my code for me?</br>
> A: Probably not unless you pay me.  I am happy to answer pointed and topical questions, but 
> I may refer you to a book, a forum, or some other resource.


## PERFORMANCE
    (on Athlon XP 2100+, with gcc 2.96, float data type)

Kiss performed 10000 1024-pt cpx ffts in .63 s of cpu time.
For comparison, it took md5sum twice as long to process the same amount of data.
Transforming 5 minutes of CD quality audio takes less than a second (nfft=1024). 

**DO NOT:**
- use Kiss if you need the Fastest Fourier Transform in the World
- ask me to add features that will bloat the code

## UNDER THE HOOD

Kiss FFT uses a time decimation, mixed-radix, out-of-place FFT. If you give it an input buffer  
and output buffer that are the same, a temporary buffer will be created to hold the data.

No static data is used.  The core routines of kiss_fft are thread-safe (but not all of the tools directory).[

No scaling is done for the floating point version (for speed).  
Scaling is done both ways for the fixed-point version (for overflow prevention).

Optimized butterflies are used for factors 2,3,4, and 5. 

The real (i.e. not complex) optimization code only works for even length ffts.  It does two half-length
FFTs in parallel (packed into real&imag), and then combines them via twiddling.  The result is 
nfft/2+1 complex frequency bins from DC to Nyquist.  If you don't know what this means, search the web.

The fast convolution filtering uses the overlap-scrap method, slightly 
modified to put the scrap at the tail.

## LICENSE
    Revised BSD License, see COPYING for verbiage. 
    Basically, "free to use&change, give credit where due, no guarantees"
    Note this license is compatible with GPL at one end of the spectrum and closed, commercial software at 
    the other end.  See http://www.fsf.org/licensing/licenses
  
## TODO
 - Add real optimization for odd length FFTs 
 - Document/revisit the input/output fft scaling
 - Make doc describing the overlap (tail) scrap fast convolution filtering in kiss_fastfir.c
 - Test all the ./tools/ code with fixed point (kiss_fastfir.c doesn't work, maybe others)

## AUTHOR
    Mark Borgerding
    Mark@Borgerding.net
