.intel_syntax noprefix
#
# x64 asm code for SmartGear
# Copyright (c) 2012-2017 BitBank Software, Inc.
# written by Larry Bank
# project started 4/24/2012
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

# Win64 calling convention: The first 4 integer parameters are passed (in left to right order) in rcx, rdx, r8, r9
# must preserve rbp, rbx, r12, r13, r14, r15
# Functions can trash: rax, rcx, rdx, r8, r9, r10, and r11 and xmm4 - xmm5
#
# Linux AMD64 call conv: The first 6 integer parameters are passed (in left to right order) in rdi, rsi, rdx, rcx, r8, r9
# must preserve rbp, rbx, r12, r13, r14, r15
# Functions can trash: rax, rcx, rdx, r8, r9, r10, r11, xmm0-xmm15, mmx0-mmx7
#
 .global X86DrawScaled32_0

 .text
#
# called from C as:
#                             rdi         rsi          rdx         rcx
#                             rcx         rdx          r8          r9
# void X86DrawScaled32_0(void *pSrc, void *pDest, int iWidth, int iScaleX)
# register usage
X86DrawScaled32_0:
	xor     r10,r10					# scale accumulator
drawscaled32_loop:
    mov rax,r10
	shr rax,8
	mov eax,DWORD PTR [rdi + rax*4]	# source pixel 1
	add r10,rcx				# add scale to accumulator
	pinsrd xmm0,eax,0		# use SSE registers to hold on to the data
	mov rax,r10
	shr rax,8
	mov eax,DWORD PTR [rdi + rax*4]	# source pixel 2
	add r10,rcx
	pinsrd xmm0,eax,1
	mov rax,r10
	shr rax,8
	mov eax,DWORD PTR [rdi + rax*4] # source pixel 3
	add r10,rcx
	pinsrd xmm0,eax,2
	mov rax,r10
	shr rax,8
	mov eax,DWORD PTR [rdi + rax*4] # source pixel 4
	add r10,rcx
	pinsrd xmm0,eax,3
	movdqu XMMWORD PTR [rsi],xmm0	# store the 4 pixels
	add rsi,16
    sub rdx,4		# do 4 at a time
	jg  drawscaled32_loop
    and rdx,3		# any remaining pixels?
	je drawscaled32_done
drawscaled32_loop_2:
    mov rax,r10
	shr rax,8
	mov eax,DWORD PTR [rdi + rax*4]
	add r10,rcx
	mov DWORD PTR [rsi], eax
	add rsi,4
	sub rdx,1
	jne drawscaled32_loop_2

drawscaled32_done:
	ret
#_TEXT	ENDS
 .end
