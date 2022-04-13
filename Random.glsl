//?#version 430
/*
    https://stackoverflow.com/questions/4200224/random-noise-functions-for-glsl
    static.frag
    by Spatial
    05 July 2013
*/
// A single iteration of Bob Jenkins' One-At-A-Time hashing algorithm.
#ifndef TIME_H
#define TIME_H
#include "Math.glsl"
uniform vec4 time;
uint hash( uint x ) {
    x += ( x << 10u );
    x ^= ( x >>  6u );
    x += ( x <<  3u );
    x ^= ( x >> 11u );
    x += ( x << 15u );
    return x;
}

// Compound versions of the hashing algorithm I whipped together.
uint hash( uvec2 v ) { return hash( v.x ^ hash(v.y)                         ); }
uint hash( uvec3 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z)             ); }
uint hash( uvec4 v ) { return hash( v.x ^ hash(v.y) ^ hash(v.z) ^ hash(v.w) ); }


// Construct a float with half-open range [0:1] using low 23 bits.
// All zeroes yields 0.0, all ones yields the next smallest representable value below 1.0.
float floatConstruct( uint m ) {
    const uint ieeeMantissa = 0x007FFFFFu; // binary32 mantissa bitmask
    const uint ieeeOne      = 0x3F800000u; // 1.0 in IEEE binary32

    m &= ieeeMantissa;                     // Keep only mantissa bits (fractional part)
    m |= ieeeOne;                          // Add fractional part to 1.0

    float  f = uintBitsToFloat( m );       // Range [1:2]
    return f - 1.0;                        // Range [0:1]
}



// Pseudo-random value in half-open range [0:1].
float random( float x ) { return floatConstruct(hash(floatBitsToUint(x))); }
float random( vec2  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
float random( vec3  v ) { return floatConstruct(hash(floatBitsToUint(v))); }
float random( vec4  v ) { return floatConstruct(hash(floatBitsToUint(v))); }


// https://github.com/BrianSharpe/GPU-Noise-Lib/blob/master/gpu_noise_lib.glsl
vec4 Interpolation_C2_InterpAndDeriv( vec2 x ) { return x.xyxy * x.xyxy * ( x.xyxy * ( x.xyxy * ( x.xyxy * vec2( 6.0, 0.0 ).xxyy + vec2( -15.0, 30.0 ).xxyy ) + vec2( 10.0, -60.0 ).xxyy ) + vec2( 0.0, 30.0 ).xxyy ); }
vec2 Interpolation_C2( vec2 x ) { return x * x * x * (x * (x * 6.0 - 15.0) + 10.0); }
vec3 Interpolation_C2( vec3 x ) { return x * x * x * (x * (x * 6.0 - 15.0) + 10.0); }   //  6x^5-15x^4+10x^3	( Quintic Curve.  As used by Perlin in Improved Noise.  http://mrl.nyu.edu/~perlin/paper445.pdf )
//
//	FAST32_hash
//	A very fast hashing function.  Requires 32bit support.
//	http://briansharpe.wordpress.com/2011/11/15/a-fast-and-simple-32bit-floating-point-hash-function/
//
//	The 2D hash formula takes the form....
//	hash = mod( coord.x * coord.x * coord.y * coord.y, SOMELARGEFLOAT ) / SOMELARGEFLOAT
//	We truncate and offset the domain to the most interesting part of the noise.
//	SOMELARGEFLOAT should be in the range of 400.0->1000.0 and needs to be hand picked.  Only some give good results.
//	A 3D hash is achieved by offsetting the SOMELARGEFLOAT value by the Z coordinate
//
vec4 FAST32_hash_2D( vec2 gridcell )	//	generates a random number for each of the 4 cell corners
{
    //	gridcell is assumed to be an integer coordinate
    const vec2 OFFSET = vec2( 26.0, 161.0 );
    const float DOMAIN = 71.0;
    const float SOMELARGEFLOAT = 951.135664;
    vec4 P = vec4( gridcell.xy, gridcell.xy + 1.0 );
    P = P - floor(P * ( 1.0 / DOMAIN )) * DOMAIN;	//	truncate the domain
    P += OFFSET.xyxy;								//	offset to interesting part of the noise
    P *= P;											//	calculate and return the hash
    return fract( P.xzxz * P.yyww * ( 1.0 / SOMELARGEFLOAT ) );
}

//	Value2D noise with derivatives
//	returns vec3( value, xderiv, yderiv )
//
vec3 Value2D_Deriv( vec2 P )
{
    //	establish our grid cell and unit position
    vec2 Pi = floor(P);
    vec2 Pf = P - Pi;

    //	calculate the hash.
    vec4 hash = FAST32_hash_2D( Pi );

    //	blend result and return
    vec4 blend = Interpolation_C2_InterpAndDeriv( Pf );
    vec4 res0 = mix( hash.xyxz, hash.zwyw, blend.yyxx );
    return vec3( res0.x, 0.0, 0.0 ) + ( res0.yyw - res0.xxz ) * blend.xzw;
}

//
//	Value Noise 2D
//	Return value range of 0.0->1.0
//	http://briansharpe.files.wordpress.com/2011/11/valuesample1.jpg
//
float Value2D( vec2 P )
{
    //	establish our grid cell and unit position
    vec2 Pi = floor(P);
    vec2 Pf = P - Pi;

    //	calculate the hash.
    //	( various hashing methods listed in order of speed )
    vec4 hash = FAST32_hash_2D( Pi );
    //vec4 hash = FAST32_2_hash_2D( Pi );
    //vec4 hash = BBS_hash_2D( Pi );
    //vec4 hash = SGPP_hash_2D( Pi );
    //vec4 hash = BBS_hash_hq_2D( Pi );

    //	blend the results and return
    vec2 blend = Interpolation_C2( Pf );
    vec4 blend2 = vec4( blend, vec2( 1.0 - blend ) );
    return dot( hash, blend2.zxzx * blend2.wwyy );
}

void FAST32_hash_3D( vec3 gridcell, out vec4 lowz_hash, out vec4 highz_hash )	//	generates a random number for each of the 8 cell corners
{
    //    gridcell is assumed to be an integer coordinate

    //	TODO: 	these constants need tweaked to find the best possible noise.
    //			probably requires some kind of brute force computational searching or something....
    const vec2 OFFSET = vec2( 50.0, 161.0 );
    const float DOMAIN = 69.0;
    const float SOMELARGEFLOAT = 635.298681;
    const float ZINC = 48.500388;

    //	truncate the domain
    gridcell.xyz = gridcell.xyz - floor(gridcell.xyz * ( 1.0 / DOMAIN )) * DOMAIN;
    vec3 gridcell_inc1 = step( gridcell, vec3( DOMAIN - 1.5 ) ) * ( gridcell + 1.0 );

    //	calculate the noise
    vec4 P = vec4( gridcell.xy, gridcell_inc1.xy ) + OFFSET.xyxy;
    P *= P;
    P = P.xzxz * P.yyww;
    highz_hash.xy = vec2( 1.0 / ( SOMELARGEFLOAT + vec2( gridcell.z, gridcell_inc1.z ) * ZINC ) );
    lowz_hash = fract( P * highz_hash.xxxx );
    highz_hash = fract( P * highz_hash.yyyy );
}
//	Value Noise 3D
//	Return value range of 0.0->1.0
//	http://briansharpe.files.wordpress.com/2011/11/valuesample1.jpg
//
float Value3D( vec3 P )
{
    //	establish our grid cell and unit position
    vec3 Pi = floor(P);
    vec3 Pf = P - Pi;

    //	calculate the hash.
    //	( various hashing methods listed in order of speed )
    vec4 hash_lowz, hash_highz;
    FAST32_hash_3D( Pi, hash_lowz, hash_highz );
    //FAST32_2_hash_3D( Pi, hash_lowz, hash_highz );
    //BBS_hash_3D( Pi, hash_lowz, hash_highz );
    //SGPP_hash_3D( Pi, hash_lowz, hash_highz );

    //	blend the results and return
    vec3 blend = Interpolation_C2( Pf );
    vec4 res0 = mix( hash_lowz, hash_highz, blend.z );
    vec4 blend2 = vec4( blend.xy, vec2( 1.0 - blend.xy ) );
    return dot( res0, blend2.zxzx * blend2.wwyy );
}

vec3 tangentFromSpherical(float theta, float phi)
{
    return vec3(
        sin(theta)*cos(phi),
        sin(theta)*sin(phi),
        cos(theta)
        );
    }

vec3 sphereTangent(vec3 normal)
{
    float theta = acos(normal.z);
    float phi = atan(normal.y,normal.x);

    //then add pi/2 to theta or phi
    float theta1 = theta + pi/2;
    vec3 tan1 = tangentFromSpherical(theta1, phi);
    if(dot(tan1,normal) == 0)
    {
        return tan1;
    }
    float phi1 = phi + pi/2;
    vec3 tan2 = tangentFromSpherical(theta, phi1);
    if(dot(tan2,normal) == 0)
    {
        return tan2;
    }
    return tangentFromSpherical(theta1, phi1);
}
const mat2 m2 = mat2(  0.80,  0.60,
                      -0.60,  0.80 );

vec4 fbmTerrain (vec2 st) {
	
    // Initial values
    float value = 0.0;
    vec2 derivates = vec2(0);
	float amplitude = 1.0;
    float lastOctave = 0.5;
    
    // Loop of octaves
    for (int i = 0; i < 9; i++) {
        vec3 randomWithDerivates = Value2D_Deriv(st);
		vec3 n = randomWithDerivates * amplitude;
        if(i>=8)
        {
            lastOctave *= randomWithDerivates.x; 
        }
        value += n.x/(1.0+dot(derivates,derivates));
		derivates += n.yz;
        st *= m2*2.0;//lacunarity
        amplitude *= 0.5;//gain
    }
    return vec4(value, derivates.x, derivates.y, lastOctave);
}

#endif