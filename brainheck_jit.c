/* NOTE: DO NOT USE THIS FILE.
IT EXISTS BECAUSE I WAS TRYING
TO MAKE THE MOST OPTIMIZED
VERSION I COULD FOR A CONTEST. */

/*
brainheck-jit by 0x3F

This code is licensed under GPL because it links with
GNU Lightning which uses the GPL license.
 */

//#define BOUNDS_CHECK
#define ALLOW_IO

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <lightning.h>

/* some macros */

#ifdef BOUNDS_CHECK
#define MOVE_RIGHT(amount)\
	jit_addi(JIT_V2, JIT_V2, amount);\
	buffer_test = jit_bler(JIT_V2, JIT_V3);\
	jit_reti(1);\
	inside_buffer = jit_label();\
	jit_patch_at(buffer_test, inside_buffer);
#else
#define MOVE_RIGHT(amount)\
	jit_addi(JIT_V2, JIT_V2, amount);
#endif

#ifdef BOUNDS_CHECK
#define MOVE_LEFT(amount)\
	jit_subi(JIT_V2, JIT_V2, amount);\
	buffer_test = jit_bger(JIT_V2, JIT_V1);\
	jit_reti(2);\
	inside_buffer = jit_label();\
	jit_patch_at(buffer_test, inside_buffer);
#else
#define MOVE_LEFT(amount)\
	jit_subi(JIT_V2, JIT_V2, amount);
#endif

#define ADD(amount)\
jit_ldr_uc(JIT_R0, JIT_V2);\
jit_addi(JIT_R0, JIT_R0, amount);\
jit_str_c(JIT_V2, JIT_R0);

#define SUB(amount)\
jit_ldr_uc(JIT_R0, JIT_V2);\
jit_subi(JIT_R0, JIT_R0, amount);\
jit_str_c(JIT_V2, JIT_R0);


typedef int (*brainjit)(unsigned char *cells, unsigned char *cell_max);

/* I guess lightning needs this to be global */
static jit_state_t *_jit = NULL;


static inline unsigned long long count_sequence(unsigned char *program, unsigned char **pcc)
{
	unsigned char *pc = program;
	unsigned long long amount = 1;

	*pcc = pc;

	if(!*pc)
		return 0;

	for(; *pc; pc++)
	{
		*pcc = pc;

		if(*pc != *(pc + 1))
			return amount;
		else
			amount++;
	}

	*pcc = pc;

	return amount;
}

static inline unsigned char *brainheck_optimize_mulloop(unsigned char *program)
{
	unsigned char *pc = program, *temp_pc = NULL;
	unsigned long amount, shift_distance;

	if(*pc != '[' && *(pc + 1) != '-' && *(pc + 2) != '>')
		return NULL;
	
	pc += 2;

	shift_distance = count_sequence(pc, &temp_pc);




	return pc;
}

static inline unsigned char *brainheck_optimize(unsigned char *program)
{
	unsigned char *pc = program, *temp_pc = NULL;

	/* optimize [-] to setting the cell to 0 */
	if(*pc == '[' && *(pc + 1) == '-' && *(pc + 2) == ']')
	{
		jit_movi(JIT_R0, 0);
		jit_str_c(JIT_V2, JIT_R0);

		pc += 3;
	}
	//else if((temp_pc = brainheck_optimize_mulloop(pc))) /* optimize multiplication loop */
	{
		//pc = temp_pc;
	}

	return pc;
}

unsigned char *brainheck_compile(unsigned char *program, unsigned long long level)
{
	unsigned char *pc = program, has_start_bracket = 0;
	unsigned long long op_counter = 1;
	jit_node_t *open, *close, *branch, *inside_buffer, *buffer_test;

	/* run here first _and_ in the loop */
	if(!(pc = brainheck_optimize(pc)))
		return NULL;

	if(*pc == '[' && level)
	{
		has_start_bracket = 1;
		//printf("open bracket\n");
		/* make branch then label. The branch has to use a forward label */

		jit_ldr_uc(JIT_R0, JIT_V2);
		/* make branch */
		close = jit_forward();
		branch = jit_beqi(JIT_R0, 0);

		/* finally make label */
		open = jit_label();

		pc++;
	}
	else if(*pc == '[' && !level)
	{
		if(!(pc = brainheck_compile(pc, level + 1)))
			return NULL;

		pc++;
	}

	for(; *pc; pc++)
	{
		if(!(pc = brainheck_optimize(pc)))
			return NULL;
		
		if(*pc != *(pc + 1)) /* condense */
		{
			switch(*pc)
			{
				case '>':
					MOVE_RIGHT(op_counter);
				break;
				case '<':
					MOVE_LEFT(op_counter);
				break;
				case '+':
					ADD(op_counter);
				break;
				case '-':
					SUB(op_counter);
				break;
			}

			op_counter = 1;
		}
		else if(*pc == '+' || *pc == '-' || *pc == '<' || *pc == '>')
			op_counter++;

		switch(*pc)
		{
			case '.':
				#ifdef ALLOW_IO
				jit_prepare();
				jit_ldr_uc(JIT_R0, JIT_V2);
				jit_pushargr(JIT_R0);
				jit_finishi(putchar);
				#endif
			break;
			case ',':
				#ifdef ALLOW_IO
				jit_prepare();
				jit_finishi(getchar);
				jit_retval(JIT_R0);
				jit_str_c(JIT_V2, JIT_R0);
				#endif
			break;
			case '[': /* dont generate anycode. Thats done at the start */
				if(!(pc = brainheck_compile(pc, level + 1)))
					return NULL;
			break;
			case ']': /* connect the open bracket forward label and the closing bracket with the start label */
				if(has_start_bracket)
				{
					jit_ldr_uc(JIT_R0, JIT_V2);
					/* link the instr after the branch */
					jit_patch_at(jit_bnei(JIT_R0, 0), open);

					/* link the branch to the instruction after this */
					jit_patch_at(branch, close);
					jit_link(close);

					return pc;
				}
				else
				{
					fprintf(stderr, "unmatched bracket\n");
					return NULL;
					exit(1);
				}
			break;
		}

	}

	return pc;
}


int brainheck(unsigned char *program, unsigned char *cells, unsigned long long cell_max)
{
	unsigned char *cell_ptr = cells, *pc = program;
	unsigned int ret = 0;
	brainjit bjit;
	//jit_node_t *arg, *arg2;

	_jit = jit_new_state();

	jit_prolog();

	/* store the cell pointer in v1, cell pointer again in v2 (as offset), and the maximum pointer in v3 */
	jit_getarg(JIT_V1, jit_arg());
	jit_getarg(JIT_V3, jit_arg());
	//jit_movi(JIT_V1, cells);
	//jit_movi(JIT_V3, cells + cell_max);


	/* copy the value in v1 into v2 */
	jit_movr(JIT_V2, JIT_V1);

	/* NOTE return early to test addresses */
	//jit_retr(JIT_R0);
	
	if(!brainheck_compile(program, 0))
	{
		fprintf(stderr, "problem compiling input\n");
		return 1;
	}

	//free(program);
	

	/* return 0 */
	jit_reti(0);


	bjit = jit_emit();
	jit_clear_state();

	/* execute generated code */
	switch((ret = bjit(cells, cells + cell_max)))
	{
		case 1:
			fprintf(stderr, "ran out of cells in positive direction\n");
		break;
		case 2:
			fprintf(stderr, "ran out of cells in negative direction\n");
		break;
	}
	

	/* jit_disassemble(); */

	jit_destroy_state();
	finish_jit();

	for(int i = 0; i < 10; i++)
	{
		printf(" %u ", cells[i]);
	}
	printf("\n");

	return 0;
}

int main(int argc, char **argv)
{
	FILE *file = NULL;
	unsigned long size = 0, i, begin;
	char *file_program = NULL, *program = NULL, *default_program = "--[----->+<]>----.[--->+<]>----.+++[->+++<]>++.++++++++.+++++.------.---.--.++++++++.+++[----->++<]>+.---[-->+++++<]>+.-.+++++++++++.[---->+<]>+++.-[--->++<]>--.---.+++++++.++++.>++++++++++..[------->+++<]>.+++.-------.-[->+++++<]>-.++[->+++<]>.++++++++++++.---.--.[->+++++<]>-.++[->+++<]>.+++.+++.-------.+++[->+++<]>++.>++++++++++.-[->+++++<]>+.+.++[->++<]>.[--->+<]>----.+++[->+++<]>++.++++++++.+++++.------.---.--.++++++++.------------.+++++++++++.-.+++++++++++.[---->+<]>+++.[-->+++++++<]>.[----->++<]>+.--[--->+<]>-.------------.---[->+++<]>.-[--->++<]>.-----.+[->+++++<]>-.[--->+<]>+.+++.+++.-------.[->+++<]>-.+[--->+<]>.[--->+<]>+.----.>++++++++++..";

	#ifdef _WIN32
	freopen("CON", "w", stderr);
	freopen("CON", "w", stdout);
	freopen("CON", "r", stdin);
	#endif

	init_jit(argv[0]);

	if(argc == 2)
	{
		if(!(file = fopen(argv[1], "r")))
		{
			fprintf(stderr, "could not open file\n");
			return 1;
		}
		
		fseek(file, 0, SEEK_END);
		size = ftell(file);
		fseek(file, 0, SEEK_SET);

		if(!(program = malloc(size + 1))) /* TODO change this to malloc for speed */
		{
			fprintf(stderr, "could allocate file\n");
			fclose(file);
			
			return 1;
		}

		program[size + 1] = 0x00;

		if(fread(program, 1, size, file) != size)
		{
			fprintf(stderr, "could not read file\n");
			fclose(file);
			
			return 1;
		}

		fclose(file);
	}
	else
		program = default_program;

	if(brainheck(program, (unsigned char[30000]){0}, 30000))
			fprintf(stderr, "error in input\n");


	return 0;
}