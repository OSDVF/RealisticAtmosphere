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


// https://www.shadertoy.com/view/4dffRH

vec3 hash( vec3 p ) // replace this by something better. really. do
{
	p = vec3( dot(p,vec3(127.1,311.7, 74.7)),
			  dot(p,vec3(269.5,183.3,246.1)),
			  dot(p,vec3(113.5,271.9,124.6)));

	return -1.0 + 2.0*fract(sin(p)*43758.5453123);
}

// return value noise (in x) and its derivatives (in yzw)
vec4 noised( in vec3 x )
{
    // grid
    vec3 i = floor(x);
    vec3 w = fract(x);
    
    #if 1
    // quintic interpolant
    vec3 u = w*w*w*(w*(w*6.0-15.0)+10.0);
    vec3 du = 30.0*w*w*(w*(w-2.0)+1.0);
    #else
    // cubic interpolant
    vec3 u = w*w*(3.0-2.0*w);
    vec3 du = 6.0*w*(1.0-w);
    #endif    
    
    // gradients
    vec3 ga = hash( i+vec3(0.0,0.0,0.0) );
    vec3 gb = hash( i+vec3(1.0,0.0,0.0) );
    vec3 gc = hash( i+vec3(0.0,1.0,0.0) );
    vec3 gd = hash( i+vec3(1.0,1.0,0.0) );
    vec3 ge = hash( i+vec3(0.0,0.0,1.0) );
	vec3 gf = hash( i+vec3(1.0,0.0,1.0) );
    vec3 gg = hash( i+vec3(0.0,1.0,1.0) );
    vec3 gh = hash( i+vec3(1.0,1.0,1.0) );
    
    // projections
    float va = dot( ga, w-vec3(0.0,0.0,0.0) );
    float vb = dot( gb, w-vec3(1.0,0.0,0.0) );
    float vc = dot( gc, w-vec3(0.0,1.0,0.0) );
    float vd = dot( gd, w-vec3(1.0,1.0,0.0) );
    float ve = dot( ge, w-vec3(0.0,0.0,1.0) );
    float vf = dot( gf, w-vec3(1.0,0.0,1.0) );
    float vg = dot( gg, w-vec3(0.0,1.0,1.0) );
    float vh = dot( gh, w-vec3(1.0,1.0,1.0) );
	
    // interpolations
    return vec4(0.5,0,0,0) + vec4(0.5,1,1,1) * vec4( va + u.x*(vb-va) + u.y*(vc-va) + u.z*(ve-va) + u.x*u.y*(va-vb-vc+vd) + u.y*u.z*(va-vc-ve+vg) + u.z*u.x*(va-vb-ve+vf) + (-va+vb+vc-vd+ve-vf-vg+vh)*u.x*u.y*u.z,    // value
                 ga + u.x*(gb-ga) + u.y*(gc-ga) + u.z*(ge-ga) + u.x*u.y*(ga-gb-gc+gd) + u.y*u.z*(ga-gc-ge+gg) + u.z*u.x*(ga-gb-ge+gf) + (-ga+gb+gc-gd+ge-gf-gg+gh)*u.x*u.y*u.z +   // derivatives
                 du * (vec3(vb,vc,ve) - va + u.yzx*vec3(va-vb-vc+vd,va-vc-ve+vg,va-vb-ve+vf) + u.zxy*vec3(va-vb-ve+vf,va-vb-vc+vd,va-vc-ve+vg) + u.yzx*u.zxy*(-va+vb+vc-vd+ve-vf-vg+vh) ));
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

const float pi = atan(1.0) * 4.0;

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

#endif