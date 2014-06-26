#include <windows.h>

#include "include/nes.h"

using namespace std;

#include <time.h>
#include <stdio.h>
void zero(FILE *fp, int n)
{
	for (int i = 0; i < n; i++)
		fputc(0, fp);
}

int main(int argc, char **argv)
{
	if (argc < 2)
		return 0;

	FILE *fp = fopen(argv[1], "r");
	if (!fp)
		return 1;

	fseek(fp, 0, SEEK_END);
	int srcsize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	string src;
	src.resize(srcsize);
	src.resize(fread(&src[0], 1, srcsize, fp));
	fclose(fp);

	using namespace NES;
	Environment env = nes::compile_IL(src);
	if (!env)
		return 0;

	size_t code = env->getCodeSize();
	size_t code_f = (code + 0x1FF)/0x200*0x200;
	size_t codeimage = (code_f + 0xFFF)/0x1000*0x1000;

	IL::Environment::Native n = env->gen(0x401000, 0x401000 + codeimage);
	if (!n)
		return 0;
	size_t global = n->global.size();
	size_t global_f = (global + 0x1FF)/0x200*0x200;
	size_t dataimage = (global_f + 0xFFF)/0x1000*0x1000;
	const char dos[] = {0x0E,0x1F,0xBA,0x0E,0x00,0xB4,0x09,0xCD,0x21,0xB8,0x01,0x4C,0xCD,0x21,0x44,0x4F,0x53,0x0D,0x0D,0x0A,0x24};
	IMAGE_DOS_HEADER dos_header = {0x5A4D, 0x90, 3, 0, 4, 0, 0xFFFF, 0, 0xB8, 0, 0, 0, 0x40, 0, {0}, 0, 0, {0}, sizeof(IMAGE_DOS_HEADER) + ((sizeof(dos)+15)/16)*16};
	IMAGE_NT_HEADERS32 nt_header = {0x4550,
		{0x14C, 2, time(0), 0, 0, sizeof(IMAGE_OPTIONAL_HEADER32),
			IMAGE_FILE_RELOCS_STRIPPED|IMAGE_FILE_EXECUTABLE_IMAGE|IMAGE_FILE_32BIT_MACHINE},
		{0x10B, 1, 0,
			codeimage, // SizeOfCode
			dataimage, // SizeOfInitializedData
			0,0x1000+n->function_address["main"],0x1000,
			0x1000 + codeimage, // BaseOfData
			0x400000,0x1000,0x200,4,0,0,0,4,0,0,
			0x1000 + codeimage + dataimage, // SizeOfImage
			0x200, // SizeOfHeaders
			0,IMAGE_SUBSYSTEM_WINDOWS_CUI,0,0x100000,0x1000,0x100000,0,0,IMAGE_NUMBEROF_DIRECTORY_ENTRIES,{
				{0,0},
				{0x1800+codeimage,sizeof(IMAGE_IMPORT_DESCRIPTOR)},
				{0,0},
				{0,0},
				{0,0},
				{0,0},
				{0,0},
				{0,0},
				{0,0},
				{0,0},
				{0,0},
				{0,0},
				{0x1000+codeimage,12},
				{0,0},
				{0,0},
				{0,0},
			}
		}
	};

	IMAGE_SECTION_HEADER text_section = {".text", codeimage, 0x1000, code_f, 0x200, 0, 0, 0, 0, IMAGE_SCN_CNT_CODE|IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_READ};
	IMAGE_SECTION_HEADER data_section = {".data", dataimage, 0x1000+codeimage, global_f, 0x200+code_f, 0, 0, 0, 0, IMAGE_SCN_CNT_INITIALIZED_DATA|IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE};

	string exe(argv[1]);
	exe += ".exe";
	if (FILE *fp = fopen(exe.c_str(), "wb"))
	{
		fwrite(&dos_header, sizeof(dos_header), 1, fp);
		fwrite(dos, sizeof(dos), 1, fp);
		if (int m = sizeof(dos) % 16)
			zero(fp, 16-m);
		fwrite(&nt_header, sizeof(nt_header), 1, fp);
		fwrite(&text_section, sizeof(text_section), 1, fp);
		fwrite(&data_section, sizeof(data_section), 1, fp);

		fseek(fp, 0x200+code_f, SEEK_SET);
		for (size_t i = 0; i < n->global.size(); i++)
		{
			fputc(n->global[i], fp);
		}
		if (global < global_f)
		{
			fseek(fp, 0x200 + code_f + global_f - 1, SEEK_SET);
			fputc(0, fp);
		}

		{
			IMAGE_IMPORT_DESCRIPTOR import_desc = {0x1C00+codeimage, 0, 0, 0x1814+codeimage, 0x1000+codeimage};
			const char dllname[] = "msvcrt.dll";
			int table[] = {
				0x1830+codeimage-2,
				0x1840+codeimage-2,
				0x1850+codeimage-2,
				0};

			fseek(fp, 0xA00+code_f, SEEK_SET);
			fwrite(&import_desc, sizeof(IMAGE_IMPORT_DESCRIPTOR), 1, fp);
			fprintf(fp, "%s", dllname);
			fseek(fp, 0xA30+code_f, SEEK_SET);fprintf(fp, "%s", "putchar");
			fseek(fp, 0xA40+code_f, SEEK_SET);fprintf(fp, "%s", "getchar");
			fseek(fp, 0xA50+code_f, SEEK_SET);fprintf(fp, "%s", "puts");

			fseek(fp, 0xE00+code_f, SEEK_SET);
			fwrite(table, sizeof(table), 1, fp);
			fseek(fp, 0x200+code_f, SEEK_SET);
			fwrite(table, sizeof(table), 1, fp);
		}

		fseek(fp, 0x200, SEEK_SET);
		for (size_t i = 0; i < n->code.size(); i++)
		{
			fputc(n->code[i], fp);
		}

		fclose(fp);
	}

	return 0;
}
