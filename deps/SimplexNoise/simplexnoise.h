// SimplexNoise1234
// Copyright © 2003-2011, Stefan Gustavson
//
// Contact: stegu@itn.liu.se
//
// This library is public domain software, released by the author
// into the public domain in February 2011. You may do anything
// you like with it. You may even remove all attributions,
// but of course I'd appreciate it if you kept my name somewhere.
//
// This library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.

/** \file
		\brief Declares the SimplexNoise1234 class for producing Perlin simplex noise.
		\author Stefan Gustavson (stegu@itn.liu.se)
*/

/*
 * This is a clean, fast, modern and free Perlin Simplex noise class in C++.
 * Being a stand-alone class with no external dependencies, it is
 * highly reusable without source code modifications.
 *
 *
 * Note:
 * Replacing the "float" type with "double" can actually make this run faster
 * on some platforms. A templatized version of SimplexNoise1234 could be useful.
 */

typedef struct {
	unsigned char perm[512];
} snoise_permtable;

void snoise_setup_perm(snoise_permtable* p, unsigned long seed);
void snoise_setup_perm_noseed(snoise_permtable* p);
float snoise1(snoise_permtable* c, float x);
float snoise2(snoise_permtable* c, float x, float y);
float snoise3(snoise_permtable* c, float x, float y, float z);
float snoise4(snoise_permtable* c, float x, float y, float z, float w);

void mt_init_genrand(unsigned long s);
void mt_init_by_array(unsigned long init_key[], int key_length);
unsigned long mt_genrand_int32(void);
long mt_genrand_int31(void);
double mt_genrand_real1(void);
double mt_genrand_real2(void);
double mt_genrand_real3(void);
double mt_genrand_res53(void);
