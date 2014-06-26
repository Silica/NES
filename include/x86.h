#ifndef NES_X86_H
#define NES_X86_H

#include <vector>

namespace NES{

struct x86
{
	typedef unsigned char byte;
	typedef unsigned long dword;
	typedef std::vector<byte> code;
	static void write8(code &c, byte b)	{c.push_back(b);}
	static void write32(code &c, dword i)	{c.push_back(i);c.push_back(i>>8);c.push_back(i>>16);c.push_back(i>>24);}

	static void mov_eax_stack(code &c, int stack){write8(c, 0x8B);write8(c, 0x85);write32(c, stack);}	// mov eax, [ebp+stack]
	static void mov_ecx_stack(code &c, int stack){write8(c, 0x8B);write8(c, 0x8D);write32(c, stack);}	// mov ecx, [ebp+stack]
	static void mov_stack_eax(code &c, int stack){write8(c, 0x89);write8(c, 0x85);write32(c, stack);}	// mov [ebp+stack], eax
	static void mov_stack_edx(code &c, int stack){write8(c, 0x89);write8(c, 0x95);write32(c, stack);}	// mov [ebp+stack], edx

	static void mov_al_stack(code &c, int stack){write8(c, 0x8A);write8(c, 0x85);write32(c, stack);}	// mov al, [ebp+stack]
	static void mov_stack_al(code &c, int stack){write8(c, 0x88);write8(c, 0x85);write32(c, stack);}	// mov [ebp+stack], al

//	static void mov_ecx_mem(code &c, int mem){write8(c, 0x8B);write8(c, 0x0D);write32(c, mem);}	// mov ecx, [mem]
	static void mov_eax_mem(code &c, int mem){write8(c, 0xA1);write32(c, mem);}	// mov eax, [mem]
	static void mov_mem_eax(code &c, int mem){write8(c, 0xA3);write32(c, mem);}	// mov [mem], eax
	static void mov_mem_al (code &c, int mem){write8(c, 0xA2);write32(c, mem);}	// mov [mem], al

	static void push_stack(code &c, int stack){write8(c, 0xFF);write8(c, 0xB5);write32(c, stack);}	// push [ebp+stack]
	static void inc_stack (code &c, int stack){write8(c, 0xFF);write8(c, 0x85);write32(c, stack);}	// inc [ebp+stack]
	static void inc_mem   (code &c, int mem  ){write8(c, 0xFF);write8(c, 0x05);write32(c, mem);}	// inc [mem]
	static void inc_ecx   (code &c           ){write8(c, 0xFF);write8(c, 0x01);}	// inc [ecx]
	static void inc_byte_stack(code &c, int stack){write8(c, 0xFE);write8(c, 0x85);write32(c, stack);}	// inc byte ptr ss:[ebp+stack]
	static void inc_byte_mem  (code &c, int mem  ){write8(c, 0xFE);write8(c, 0x05);write32(c, mem);}	// inc byte ptr ds:[mem]
	static void inc_byte_ecx  (code &c           ){write8(c, 0xFE);write8(c, 0x01);}	// inc byte ptr ds:[ecx]
	static void dec_stack (code &c, int stack){write8(c, 0xFF);write8(c, 0x8D);write32(c, stack);}	// dec [ebp+stack]
	static void dec_mem   (code &c, int mem  ){write8(c, 0xFF);write8(c, 0x0D);write32(c, mem);}	// dec [mem]
	static void dec_ecx   (code &c           ){write8(c, 0xFF);write8(c, 0x09);}	// dec [ecx]
	static void dec_byte_stack(code &c, int stack){write8(c, 0xFE);write8(c, 0x8D);write32(c, stack);}	// dec byte ptr ss:[ebp+stack]
	static void dec_byte_mem  (code &c, int mem  ){write8(c, 0xFE);write8(c, 0x0D);write32(c, mem);}	// dec byte ptr ds:[mem]
	static void dec_byte_ecx  (code &c           ){write8(c, 0xFE);write8(c, 0x09);}	// dec byte ptr ds:[ecx]

	static void mov_stack_int(code &c, int stack, int val){write8(c, 0xC7);write8(c, 0x85);write32(c, stack);write32(c, val);}	// mov [ebp+stack], val
	static void mov_stack_char(code &c, int stack, char val){write8(c, 0xC6);write8(c, 0x85);write32(c, stack);write8(c, val);}	// mov byte ptr ss:[ebp+stack], val

//	static void neg_stack(code &c, int stack){write8(c, 0xF7);write8(c, 0x9D);write32(c, stack);}	// neg [ebp+stack]
//	static void not_stack(code &c, int stack){write8(c, 0xF7);write8(c, 0x95);write32(c, stack);}	// not [ebp+stack]
	static void neg_eax(code &c){write8(c, 0xF7);write8(c, 0xD8);}	// neg eax
	static void not_eax(code &c){write8(c, 0xF7);write8(c, 0xD0);}	// not eax

	static void add_stack_int(code &c, int stack, int val){write8(c, 0x81);write8(c, 0x85);write32(c, stack);write32(c, val);}	// add [ebp+stack], val
	static void add_mem_int  (code &c, int mem  , int val){write8(c, 0xC7);write8(c, 0x05);write32(c, mem);write32(c, val);}	// add [mem], val
	static void add_ecx_int  (code &c,            int val){write8(c, 0xC7);write8(c, 0x01);write32(c, val);}	// add [ecx], val

	static void fld_stack(code &c, int stack){write8(c, 0xD9);write8(c, 0x85);write32(c, stack);}	// fld [ebp+stack]
	static void fstp_stack(code &c, int stack){write8(c, 0xD9);write8(c, 0x9D);write32(c, stack);}	// fstp [ebp+stack]
	static void fadd_stack(code &c, int stack){write8(c, 0xD8);write8(c, 0x85);write32(c, stack);}	// fadd [ebp+stack]
	static void fsub_stack(code &c, int stack){write8(c, 0xD8);write8(c, 0xA5);write32(c, stack);}	// fsub [ebp+stack]
	static void fmul_stack(code &c, int stack){write8(c, 0xD8);write8(c, 0x8D);write32(c, stack);}	// fmul [ebp+stack]
	static void fdiv_stack(code &c, int stack){write8(c, 0xD8);write8(c, 0xB5);write32(c, stack);}	// fdiv [ebp+stack]

	static void lea_eax_stack(code &c, int stack){write8(c, 0x8D);write8(c, 0x85);write32(c, stack);}	// lea eax, [ebp+stack]
	static void lea_eax_mem  (code &c, int mem  ){write8(c, 0x8D);write8(c, 0x05);write32(c, mem);}	// lea eax, [mem]
	static void add_esp_int  (code &c, int val  ){write8(c, 0x81);write8(c, 0xC4);write32(c, val);}	// add esp, val

	static void xor_eax_1  (code &c){write8(c, 0x83);write8(c, 0xF0);write8(c, 0x01);}	// xor eax, 1
	static void xor_edx_1  (code &c){write8(c, 0x83);write8(c, 0xF2);write8(c, 0x01);}	// xor edx, 1
	//static void xor_eax_eax(code &c){write8(c, 0x31);write8(c, 0xC0);}	// xor eax, eax
	static void mov_eax_edx(code &c){write8(c, 0x89);write8(c, 0xD0);}	// mov eax, edx
	//static void test_eax_eax(code &c){write8(c, 0x85);write8(c, 0xC0);}	// test eax, eax
	static void test_al_al (code &c){write8(c, 0x84);write8(c, 0xC0);}	// test al, al
	static void xor_edx_edx(code &c){write8(c, 0x31);write8(c, 0xD2);}	// xor edx, edx
	static void mov_eax_ecx(code &c){write8(c, 0x8B);write8(c, 0x01);}	// mov eax, [ecx]
	static void mov_ecx_eax(code &c){write8(c, 0x89);write8(c, 0x01);}	// mov [ecx], eax
	static void mov_ecx_al (code &c){write8(c, 0x88);write8(c, 0x01);}	// mov [ecx], al
	static void add_eax_ecx(code &c){write8(c, 0x01);write8(c, 0xC8);}	// add eax, ecx
	static void sub_eax_ecx(code &c){write8(c, 0x29);write8(c, 0xC8);}	// sub eax, ecx
	static void mul_ecx    (code &c){write8(c, 0xF7);write8(c, 0xE1);}	// mul ecx
	static void div_ecx    (code &c){write8(c, 0xF7);write8(c, 0xF1);}	// div ecx
	static void shl_eax_ecx(code &c){write8(c, 0xD3);write8(c, 0xE0);}	// shl eax, cl
	static void shr_eax_ecx(code &c){write8(c, 0xD3);write8(c, 0xE8);}	// shr eax, cl
	static void sar_eax_ecx(code &c){write8(c, 0xD3);write8(c, 0xF8);}	// sar eax, cl
	static void and_eax_ecx(code &c){write8(c, 0x21);write8(c, 0xC8);}	// and eax, ecx
	static void  or_eax_ecx(code &c){write8(c, 0x09);write8(c, 0xC8);}	// or  eax, ecx
	static void xor_eax_ecx(code &c){write8(c, 0x31);write8(c, 0xC8);}	// xor eax, ecx
	static void cmp_eax_ecx(code &c){write8(c, 0x39);write8(c, 0xC8);}	// cmp eax, ecx
	static void cmp_al_cl  (code &c){write8(c, 0x38);write8(c, 0xC8);}	// cmp  al, cl
	//static void call_eax   (code &c){write8(c, 0xFF);write8(c, 0x10);}	// call [eax]
	static void call_eax   (code &c){write8(c, 0xFF);write8(c, 0xD0);}	// call eax

	static void push_ebp   (code &c){write8(c, 0x55);}	// push ebp
	static void mov_ebp_esp(code &c){write8(c, 0x89);write8(c, 0xE5);}	// mov ebp, esp
	static void mov_esp_ebp(code &c){write8(c, 0x89);write8(c, 0xEC);}	// mov esp, ebp
	static void pop_ebp    (code &c){write8(c, 0x5D);}	// pop ebp
	static void retn       (code &c){write8(c, 0xC3);}	// retn

	static void jmp(code &c, int j){write8(c, 0xE9);write32(c, j);}	// jmp j
	static void jnz(code &c, int j){write8(c, 0x0F);write8(c, 0x85);write32(c, j);}	// jnz j
	static void je (code &c, int j){write8(c, 0x0F);write8(c, 0x84);write32(c, j);}	// je j
	static void jl3(code &c){write8(c, 0x7C);write8(c, 0x03);}	// jl short 3
	static void jle3(code &c){write8(c, 0x7E);write8(c, 0x03);}	// jle short 3
	static void jg3(code &c){write8(c, 0x7F);write8(c, 0x03);}	// jg short 3
	static void jge3(code &c){write8(c, 0x7D);write8(c, 0x03);}	// jge short 3
	static void jb3(code &c){write8(c, 0x72);write8(c, 0x03);}	// jb short 3
	static void jbe3(code &c){write8(c, 0x76);write8(c, 0x03);}	// jbe short 3
	static void ja3(code &c){write8(c, 0x77);write8(c, 0x03);}	// ja short 3
	static void jae3(code &c){write8(c, 0x73);write8(c, 0x03);}	// jae short 3
	static void je3(code &c){write8(c, 0x74);write8(c, 0x03);}	// je short 3
	static void jnz3(code &c){write8(c, 0x75);write8(c, 0x03);}	// jnz short 3

};

}
#endif
