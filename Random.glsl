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


// https://www.shadertoy.com/view/4dXBRH
float hash( in vec2 p )  // replace this by something better
{
    p  = 50.0*fract( p*0.3183099 + vec2(0.71,0.113));
    return -1.0+2.0*fract( p.x*p.y*(p.x+p.y) );
}

// return value noise (in x) and its derivatives (in yz)
vec3 noised( in vec2 p )
{
    vec2 i = floor( p );
    vec2 f = fract( p );
	
#if 1
    // quintic interpolation
    vec2 u = f*f*f*(f*(f*6.0-15.0)+10.0);
    vec2 du = 30.0*f*f*(f*(f-2.0)+1.0);
#else
    // cubic interpolation
    vec2 u = f*f*(3.0-2.0*f);
    vec2 du = 6.0*f*(1.0-f);
#endif    
    
    float va = hash( i + vec2(0.0,0.0) );
    float vb = hash( i + vec2(1.0,0.0) );
    float vc = hash( i + vec2(0.0,1.0) );
    float vd = hash( i + vec2(1.0,1.0) );
    
    float k0 = va;
    float k1 = vb - va;
    float k2 = vc - va;
    float k4 = va - vb - vc + vd;

    return vec3( va+(vb-va)*u.x+(vc-va)*u.y+(va-vb-vc+vd)*u.x*u.y, // value
                du*(u.yx*(va-vb-vc+vd) + vec2(vb,vc) - va) );     // derivative                
}


float hash(float p) { p = fract(p * 0.011); p *= p + 7.5; p *= p + p; return fract(p); }

float noise(vec3 x) {
    const vec3 step = vec3(110, 241, 171);

    vec3 i = floor(x);
    vec3 f = fract(x);
 
    // For performance, compute the base input to a 1D hash from the integer part of the argument and the 
    // incremental change to the 1D based on the 3D -> 1D wrapping
    float n = dot(i, step);

    vec3 u = f * f * (3.0 - 2.0 * f);
    return mix(mix(mix( hash(n + dot(step, vec3(0, 0, 0))), hash(n + dot(step, vec3(1, 0, 0))), u.x),
                   mix( hash(n + dot(step, vec3(0, 1, 0))), hash(n + dot(step, vec3(1, 1, 0))), u.x), u.y),
               mix(mix( hash(n + dot(step, vec3(0, 0, 1))), hash(n + dot(step, vec3(1, 0, 1))), u.x),
                   mix( hash(n + dot(step, vec3(0, 1, 1))), hash(n + dot(step, vec3(1, 1, 1))), u.x), u.y), u.z);
}

// Fractal Brownian Motion
float fbm6(vec3 x) {
    const int noise_octaves_count = 6;
	float v = 0.0;
	float a = 0.5;
	vec3 shift = vec3(100);
	for (int i = 0; i < noise_octaves_count; ++i) {
		v += a * noise(x);
		x = x * 2.0 + shift;
		a *= 0.5;
	}
	return v;
}

float hash1( vec2 p )
{
    p  = 50.0*fract( p*0.3183099 );
    return fract( p.x*p.y*(p.x+p.y) );
}
float noise( in vec2 x )
{
    vec2 p = floor(x);
    vec2 w = fract(x);
    #if 1
    vec2 u = w*w*w*(w*(w*6.0-15.0)+10.0);
    #else
    vec2 u = w*w*(3.0-2.0*w);
    #endif

    float a = hash1(p+vec2(0,0));
    float b = hash1(p+vec2(1,0));
    float c = hash1(p+vec2(0,1));
    float d = hash1(p+vec2(1,1));
    
    return -1.0+2.0*(a + (b-a)*u.x + (c-a)*u.y + (a - b - c + d)*u.x*u.y);
}
const mat2 m2 = mat2(  0.80,  0.60,
                      -0.60,  0.80 );
float fbm_9( in vec2 x )
{
    float f = 1.9;
    float s = 0.55;
    float a = 0.0;
    float b = 0.5;
    for( int i=0; i<9; i++ )
    {
        float n = noise(x);
        a += b*n;
        b *= s;
        x = f*m2*x;
    }
    
	return a;
}

vec2 terrainMap( in vec2 p )
{
    float e = fbm_9( p + vec2(1.0,-2.0) );
    float a = 1.0-smoothstep( 0.12, 0.13, abs(e+0.12) ); // flag high-slope areas (-0.25, 0.0)
    e += 0.15*smoothstep( -0.08, -0.01, e );
    return vec2(e,a);
}

vec2 tcToAngle(vec2 tc)
{
    return tc * 2 * pi;
}

vec2 angleToTc(vec2 angle)
{
    vec2 x = angle/pi;
    return vec2(x.x/2,x.y);
}

const vec3 up = vec3(0,1,0);
const vec3 right = vec3(1,0,0);
vec2 angles(vec3 pos)/*x from 0 to 2pi, y from 0 to pi*/
{
    /*vec2 tc1to1 = (2*tc)-1;

	float x = cos(pi * tc1to1.x);
    float y = sin(pi * tc1to1.x);
    float z = cos(pi * tc1to1.y);
    return vec3(x,y,z);*/

    vec3 projectedToPlane = pos;
    projectedToPlane.y = 0;

    float angleYcos = dot(normalize(pos),up);
    float angleXcos = dot(normalize(projectedToPlane),right);
    float angleY = acos(angleYcos);
    float angleX = acos(angleXcos);
    /*if(pos.x<0)
    {
        angleY+=pi;
    }*/
    if(pos.z<0)
    {
        angleX+=pi;
    }
    
    return vec2(angleX, angleY);
}

mat4 rotationX( float angle ) {
	return mat4(	1.0,		0,			0,			0,
			 		0, 	cos(angle),	-sin(angle),		0,
					0, 	sin(angle),	 cos(angle),		0,
					0, 			0,			  0, 		1);
}

mat4 rotationY( float angle ) {
	return mat4(	cos(angle),		0,		sin(angle),	0,
			 				0,		1.0,			 0,	0,
					-sin(angle),	0,		cos(angle),	0,
							0, 		0,				0,	1);
}

mat4 rotationZ( float angle ) {
	return mat4(	cos(angle),		-sin(angle),	0,	0,
			 		sin(angle),		cos(angle),		0,	0,
							0,				0,		1,	0,
							0,				0,		0,	1);
}

mat4 rotationMatrix(vec3 axis, float angle)
{
    axis = normalize(axis);
    float s = sin(angle);
    float c = cos(angle);
    float oc = 1.0 - c;
    
    return mat4(oc * axis.x * axis.x + c,           oc * axis.x * axis.y - axis.z * s,  oc * axis.z * axis.x + axis.y * s,  0.0,
                oc * axis.x * axis.y + axis.z * s,  oc * axis.y * axis.y + c,           oc * axis.y * axis.z - axis.x * s,  0.0,
                oc * axis.z * axis.x - axis.y * s,  oc * axis.y * axis.z + axis.x * s,  oc * axis.z * axis.z + c,           0.0,
                0.0,                                0.0,                                0.0,                                1.0);
}

vec3 angleToPos(vec2 angle)
{
    vec4 rotated = vec4(up,0) * rotationZ(angle.y);
    rotated = rotated * rotationY(angle.x);
    return rotated.xyz;
}

vec2 toUV(in vec3 n) 
{		
	vec2 uv;
	
	uv.x = atan(-n.x, n.y);
	uv.x = (uv.x + pi / 2.0) / (pi * 2.0) + pi * (28.670 / 360.0);
	
	uv.y = acos(n.z) / pi;
	
	return uv;
}

// Uv range: [0, 1]
vec3 toPolar(in vec2 uv)
{	
	float theta = 2.0 * pi * uv.x + - pi / 2.0;
	float phi = pi * uv.y;
	
	vec3 n;
	n.x = cos(theta) * sin(phi);
	n.y = sin(theta) * sin(phi);
	n.z = cos(phi);
	
	//n = normalize(n);
	return n;
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

float terrainElevation(vec3 p)
{
    /*float elev = 1.0 - fbm6(p*200);
    return (elev*elev*elev-0.00031)/0.81415;*/
    vec2 uv = toUV(p);
    return (terrainMap(uv*10).r+0.94)/(1.01712+0.94);
}


vec3 fbmTerrain (vec2 st) {
	
    // Initial values
    float value = 0.0;
    vec2 derivates = vec2(0);
	float amplitude = 1.0;
    
    // Loop of octaves
    for (int i = 0; i < 9; i++) {
		vec3 n = noised(st) * amplitude;
        value += n.x/(1.0+dot(derivates,derivates));
		derivates += n.yz;
        st *= m2*2.0;//lacunarity
        amplitude *= 0.5;//gain
    }
    return vec3(value,derivates.x,derivates.y);
}

vec3 terrainMap2( in vec2 p )
{
	vec3 n = fbmTerrain(p);
    float e = n.x;
    float a = 1.0-smoothstep( 0.12, 0.13, abs(e+0.12) ); // flag high-slope areas (-0.25, 0.0)
    e += 0.15*smoothstep( -0.08, -0.01, e );
    return vec3(e,n.yz);
}

#endif