ps_2_x
def c0, 1, 1, -1, 0
def c12, 0.5, 8.0, 0.0001, 0
dcl_centroid t0.xyz
dcl_centroid t1.xyz
dcl_centroid t2
dcl_centroid t3
dcl_centroid t4
dcl_centroid t5
dcl_2d s0
dcl_2d s1
dcl_2d s2
mov r1.xyz, t0
mov r1.w, c0.x
dp4 r5.x, c13, r1
dp4 r5.y, c14, r1
dp4 r5.z, c15, r1
dp4 r5.w, c16, r1
mad r5.xy, r5, c12.x, r5.w
dp4 r6.x, c17, r1
dp4 r6.y, c18, r1
dp4 r6.z, c19, r1
dp4 r6.w, c20, r1
mad r6.xy, r6, c12.x, r6.w
dp4 r7.x, t2, r1
dp4 r7.y, t3, r1
dp4 r7.z, t4, r1
dp4 r7.w, t5, r1
texldp r0, r5, s0
texldp r2, r7, s1
texldp r3, r6, s2
add r8.x, r5.w, -r5.x
add r8.y, r5.w, -r5.y
min r8.x, r8.x, r5.x
min r8.x, r8.x, r5.y
rcp r8.y, r5.w
mul r5.z, r8.x, r8.y
mul_sat r5.z, r5.z, c12.y
add r8.x, r7.w, -r7.x
add r8.y, r7.w, -r7.y
min r8.x, r8.x, r7.x
min r8.x, r8.x, r7.y
rcp r8.y, r7.w
mul r7.z, r8.x, r8.y
mul_sat r7.z, r7.z, c12.y
add r8.x, r6.w, -r6.x
add r8.y, r6.w, -r6.y
min r8.x, r8.x, r6.x
min r8.x, r8.x, r6.y
rcp r8.y, r6.w
mul r6.z, r8.x, r8.y
mul_sat r6.z, r6.z, c12.y
mul r0.xyz, r0, r5.z
mad r0.xyz, r2, r7.z, r0
mad r0.xyz, r3, r6.z, r0
add r8.x, r5.z, r7.z
add r8.x, r8.x, r6.z
max r8.y, r8.x, c12.z
rcp r8.y, r8.y
mul r0.xyz, r0, r8.y
add r8.x, r8.x, -c12.z
cmp r0.xyz, r8.x, r0, c7
add r1.xyz, -t1, c1
dp3 r2.x, r1, r1
rsq r0.w, r2.x
mul r1.xyz, r1, r0.w
rcp r4.w, r0.w
mov_sat r0.w, r1.z
add r1.w, -r0.w, c0.x
mul r2.w, r1.w, r1.w
add r0.xyz, r0, -c7
mul r2.w, r2.w, r2.w
mov r2.xyz, c7
mad_pp r0.xyz, c8.y, r0, r2
mul r2.w, r1.w, r2.w
mul r1.xyz, r1, -c0
mov r1.w, c0.x
add r1.w, r1.w, -c11.x
mad r2.w, r1.w, r2.w, c11.x
dp3_sat r2.x, r1, c2
mov r1.xyz, c6
add r1.xyz, -r1, c5
pow r1.w, r2.x, c8.x
mad_pp r2.xyz, r0.w, r1, c6
mov_sat r3.w, c2.w
lrp r1.xyz, r2.w, r0, r2
add r0.w, -r4.w, c9.x
mul r0.xyz, r1.w, c3
rcp r1.w, c9.y
mad r1.xyz, r0, r3.w, r1
mul_sat r0.w, r0.w, r1.w
add r0.xyz, -r1, c10
add r1.w, -r0.w, c0.x
max r0.w, c8.z, r2.w
mad r0.xyz, r1.w, r0, r1
mov oC0, r0
