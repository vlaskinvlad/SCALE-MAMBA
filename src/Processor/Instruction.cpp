/*
Copyright (c) 2017, The University of Bristol, Senate House, Tyndall Avenue, Bristol, BS8 1TH, United Kingdom.
Copyright (c) 2019, COSIC-KU Leuven, Kasteelpark Arenberg 10, bus 2452, B-3001 Leuven-Heverlee, Belgium.

All rights reserved
*/

#include "Processor/Instruction.h"
#include "Exceptions/Exceptions.h"
#include "Offline/offline_data.h"
#include "Processor/Processor.h"
#include "Tools/Crypto.h"
#include "Tools/parse.h"

#include <algorithm>
#include <map>
#include <mutex>
#include <sstream>
#include <stdlib.h>
#include <unistd.h>

#include "GC/Garbled.h"
#include "OT/OT_Thread_Data.h"

extern OT_Thread_Data OTD;

extern Base_Circuits Global_Circuit_Store;
extern vector<sacrificed_data> SacrificeD;

using namespace std;

// Convert modp to signed bigint of a given bit length
void to_signed_bigint(bigint &bi, const gfp &x, int len)
{
  to_bigint(bi, x);
  int neg;
  // get sign and abs(x)
  bigint p_half= (gfp::pr() - 1) / 2;
  if (mpz_cmp(bi.get_mpz_t(), p_half.get_mpz_t()) < 0)
    neg= 0;
  else
    {
      bi= gfp::pr() - bi;
      neg= 1;
    }
  // reduce to range -2^(len-1), ..., 2^(len-1)
  bigint one= 1;
  bi&= (one << len) - 1;
  if (neg)
    bi= -bi;
}

void Instruction::parse(istream &s)
{
  n= 0;
  start.resize(0);
  r[0]= 0;
  r[1]= 0;
  r[2]= 0;
  r[3]= 0;

  int pos= s.tellg();
  opcode= get_int(s);
  size= opcode >> 9;
  opcode&= 0x1FF;

  if (size == 0)
    size= 1;

  parse_operands(s, pos);
}

void BaseInstruction::parse_operands(istream &s, int pos)
{
  int num_var_args= 0;
  // cout << "Parsing instruction " << hex << showbase << opcode << " at " <<
  // dec << pos << endl;
  switch (opcode)
    {
      // instructions with 3 register (or 3 integer) operands
      case ADDC:
      case ADDS:
      case ADDM:
      case SUBC:
      case SUBS:
      case SUBML:
      case SUBMR:
      case MULC:
      case MULM:
      case DIVC:
      case MODC:
      case TRIPLE:
      case ANDC:
      case XORC:
      case ORC:
      case SHLC:
      case SHRC:
      case LTINT:
      case GTINT:
      case EQINT:
      case ADDINT:
      case SUBINT:
      case MULINT:
      case DIVINT:
      case RUN_TAPE:
      case ADDSINT:
      case ADDSINTC:
      case SUBSINT:
      case SUBSINTC:
      case SUBCINTS:
      case MULSINT:
      case MULSINTC:
      case DIVSINT:
      case SAND:
      case XORSB:
      case ANDSB:
      case ORSB:
      case ANDSINT:
      case ANDSINTC:
      case ORSINT:
      case ORSINTC:
      case XORSINT:
      case XORSINTC:
        r[0]= get_int(s);
        r[1]= get_int(s);
        r[2]= get_int(s);
        break;
      // instructions with 2 register operands
      case LDMCI:
      case LDMSI:
      case STMCI:
      case STMSI:
      case MOVC:
      case MOVS:
      case MOVINT:
      case LDMINTI:
      case STMINTI:
      case LEGENDREC:
      case SQUARE:
      case CONVINT:
      case LTZINT:
      case EQZINT:
      case RAND:
      case DIGESTC:
      case NEG:
      case NEGB:
      case CONVSINTSREG:
      case CONVREGSREG:
      case CONVSREGSINT:
      case OPENSINT:
      case OPENSBIT:
      case LDMSINTI:
      case STMSINTI:
      case MOVSINT:
      case INVSINT:
      case EQZSINT:
      case LTZSINT:
        r[0]= get_int(s);
        r[1]= get_int(s);
        break;
      // instructions with 1 register operand
      case BIT:
      case PRINTREGPLAIN:
      case LDTN:
      case LDARG:
      case STARG:
      case JMPI:
      case PUSHINT:
      case POPINT:
      case PRINTCHRINT:
      case PRINTSTRINT:
      case PRINTINT:
        r[0]= get_int(s);
        break;
      // instructions with 2 registers + 1 integer operand
      case ADDCI:
      case ADDSI:
      case SUBCI:
      case SUBSI:
      case SUBCFI:
      case SUBSFI:
      case MULCI:
      case MULSI:
      case DIVCI:
      case MODCI:
      case ANDCI:
      case XORCI:
      case ORCI:
      case SHLCI:
      case SHRCI:
      case SHLSINT:
      case SHRSINT:
      case NOTC:
      case CONVMODP:
      case BITSINT:
        r[0]= get_int(s);
        r[1]= get_int(s);
        n= get_int(s);
        break;
      // instructions with 2 registers + 1 integer operand
      case SINTBIT:
        r[0]= get_int(s);
        r[1]= get_int(s);
        r[2]= get_int(s);
        n= get_int(s);
        break;
      // instructions with 1 register + 1 integer operand
      case LDI:
      case LDSI:
      case LDMC:
      case LDMS:
      case STMC:
      case STMS:
      case LDMINT:
      case STMINT:
      case JMPNZ:
      case JMPEQZ:
      case PRINTREG:
      case LDINT:
      case INPUT_CLEAR:
      case OUTPUT_CLEAR:
      case INPUT_INT:
      case OPEN_CHAN:
      case OUTPUT_INT:
      case LDMSINT:
      case STMSINT:
      case LDSINT:
        r[0]= get_int(s);
        n= get_int(s);
        break;
      // instructions with 1 reg + 1 player + 1 integer operand
      case PRIVATE_INPUT:
      case PRIVATE_OUTPUT:
        r[0]= get_int(s);
        p= get_int(s);
        m= get_int(s);
        break;
      // instructions with 1 reg + 2 integer operand
      case PRINTFIXPLAIN:
        r[0]= get_int(s);
        n= get_int(s);
        m= get_int(s);
        break;
      // instructions with 1 integer operand
      case PRINTSTR:
      case PRINTCHR:
      case JMP:
      case START_TIMER:
      case STOP_TIMER:
      case CLOSE_CHAN:
      case PRINTMEM:
      case JOIN_TAPE:
        n= get_int(s);
        break;
      // instructions with no operand
      case CLEAR_MEMORY:
      case CLEAR_REGISTERS:
      case RESTART:
      case CRASH:
        break;
      // instructions with 4 register operands
      case PRINTFLOATPLAIN:
      case MUL2SINT:
        get_vector(4, start, s);
        break;
      // open instructions + read/write + GC instructions with variable length args
      case STARTOPEN:
      case STOPOPEN:
      case GC:
      case LF_CINT:
      case LF_SINT:
      case LF_REGINT:
      case LF_SREGINT:
        num_var_args= get_int(s);
        get_vector(num_var_args, start, s);
        break;
      // As above, but with a trailing int argument
      case OUTPUT_SHARE:
      case INPUT_SHARE:
        // subtract player/channel number from the number of arguments
        num_var_args= get_int(s) - 1;
        p= get_int(s);
        get_vector(num_var_args, start, s);
        break;
      case REQBL:
        n= get_int(s);
        if (n > 0 && gfp::pr() < bigint(1) << (n - 1))
          {
            cout << "Tape requires prime of bit length " << n << endl;
            throw invalid_params();
          }
        break;
      default:
        ostringstream os;
        os << "Invalid instruction " << hex << showbase << opcode << " at " << dec
           << pos;
        throw Invalid_Instruction(os.str());
    }
}

int BaseInstruction::get_reg_type() const
{
  switch (opcode)
    { // List here commands which write to a regint/sregint/sbit register or regint memory
      case LDMINT:
      case LDMINTI:
      case MOVINT:
      case POPINT:
      case LDTN:
      case LDARG:
      case INPUT_INT:
      case OPEN_CHAN:
      case RAND:
      case LDINT:
      case CONVMODP:
      case ADDINT:
      case SUBINT:
      case MULINT:
      case DIVINT:
      case LTZINT:
      case LTINT:
      case GTINT:
      case EQINT:
      case EQZINT:
      case STMINT:
      case LDMSINT:
      case LDMSINTI:
      case MOVSINT:
      case LDSINT:
      case ADDSINT:
      case ADDSINTC:
      case SUBSINT:
      case SUBSINTC:
      case SUBCINTS:
      case MULSINT:
      case MULSINTC:
      case DIVSINT:
      case SHLSINT:
      case SHRSINT:
      case NEG:
      case SAND:
      case XORSB:
      case ANDSB:
      case ORSB:
      case NEGB:
      case LTZSINT:
      case EQZSINT:
      case BITSINT:
      case SINTBIT:
      case CONVSINTSREG:
      case CONVREGSREG:
      case CONVSREGSINT:
      case OPENSINT:
      case OPENSBIT:
      case ANDSINT:
      case ANDSINTC:
      case ORSINT:
      case ORSINTC:
      case XORSINT:
      case XORSINTC:
      case INVSINT:
      case MUL2SINT:
      case GC:
      case LF_REGINT:
      case LF_SREGINT:
        return INT;
      default:
        return MODP;
    }
}

int BaseInstruction::get_max_reg(int reg_type) const
{
  if (get_reg_type() != reg_type)
    {
      return 0;
    }

  if (start.size())
    return *max_element(start.begin(), start.end()) + size;
  else
    return *max_element(r, r + 3) + size;
}

int Instruction::get_mem(RegType reg_type, SecrecyType sec_type) const
{
  if (get_reg_type() == reg_type and is_direct_memory_access(sec_type))
    return n + size;
  else
    return 0;
}

bool BaseInstruction::is_direct_memory_access(SecrecyType sec_type) const
{
  if (sec_type == SECRET)
    {
      switch (opcode)
        {
          case LDMS:
          case STMS:
          case LDMSINT:
          case STMSINT:
            return true;
          default:
            return false;
        }
    }
  else
    {
      switch (opcode)
        {
          case LDMC:
          case STMC:
          case LDMINT:
          case STMINT:
            return true;
          default:
            return false;
        }
    }
}

ostream &operator<<(ostream &s, const Instruction &instr)
{
  // Is it vectorized?
  if (instr.size != 1)
    {
      s << "V";
    }

  // Then the main opcode
  switch (instr.opcode)
    {
      case LDI:
        s << "LDI";
        break;
      case LDSI:
        s << "LDSI";
        break;
      case LDMC:
        s << "LDMC";
        break;
      case LDMS:
        s << "LDMS";
        break;
      case STMC:
        s << "STMC";
        break;
      case STMS:
        s << "STMS";
        break;
      case LDMCI:
        s << "LDMCI";
        break;
      case LDMSI:
        s << "LDMSI";
        break;
      case STMCI:
        s << "STMCI";
        break;
      case STMSI:
        s << "STMSI";
        break;
      case MOVC:
        s << "MOVC";
        break;
      case MOVS:
        s << "MOVS";
        break;
      case LDMINT:
        s << "LDMINT";
        break;
      case STMINT:
        s << "STMINT";
        break;
      case LDMINTI:
        s << "LDMINTI";
        break;
      case STMINTI:
        s << "STMINTI";
        break;
      case PUSHINT:
        s << "PUSHINT";
        break;
      case POPINT:
        s << "POPINT";
        break;
      case MOVINT:
        s << "MOVINT";
        break;
      case LDTN:
        s << "LDTN";
        break;
      case LDARG:
        s << "LDARG";
        break;
      case REQBL:
        s << "REQBL";
        break;
      case STARG:
        s << "STARG";
        break;
      case RUN_TAPE:
        s << "RUN_TAPE";
        break;
      case JOIN_TAPE:
        s << "JOIN_TAPE";
        break;
      case CRASH:
        s << "CRASH";
        break;
      case RESTART:
        s << "RESTART";
        break;
      case CLEAR_MEMORY:
        s << "CLEAR_MEMORY";
        break;
      case CLEAR_REGISTERS:
        s << "CLEAR_REGISTERS";
        break;
      case ADDC:
        s << "ADDC";
        break;
      case ADDS:
        s << "ADDS";
        break;
      case ADDM:
        s << "ADDM";
        break;
      case ADDCI:
        s << "ADDCI";
        break;
      case ADDSI:
        s << "ADDSI";
        break;
      case SUBC:
        s << "SUBC";
        break;
      case SUBS:
        s << "SUBS";
        break;
      case SUBML:
        s << "SUBML";
        break;
      case SUBMR:
        s << "SUBMR";
        break;
      case SUBCI:
        s << "SUBCI";
        break;
      case SUBSI:
        s << "SUBSI";
        break;
      case SUBCFI:
        s << "SUBCFI";
        break;
      case SUBSFI:
        s << "SUBSFI";
        break;
      case MULC:
        s << "MULC";
        break;
      case MULM:
        s << "MULM";
        break;
      case MULCI:
        s << "MULCI";
        break;
      case MULSI:
        s << "MULSI";
        break;
      case DIVC:
        s << "DIVC";
        break;
      case DIVCI:
        s << "DICI";
        break;
      case MODC:
        s << "MODC";
        break;
      case MODCI:
        s << "MODCI";
        break;
      case LEGENDREC:
        s << "LEGENDREC";
        break;
      case DIGESTC:
        s << "DIGESTC";
        break;
      case OUTPUT_CLEAR:
        s << "OUTPUT_CLEAR";
        break;
      case INPUT_CLEAR:
        s << "INPUT_CLEAR";
        break;
      case OUTPUT_SHARE:
        s << "OUTPUT_SHARE";
        break;
      case INPUT_SHARE:
        s << "INPUT_SHARE";
        break;
      case PRIVATE_INPUT:
        s << "PRIVATE_INPUT";
        break;
      case PRIVATE_OUTPUT:
        s << "PRIVATE_OUTPUT";
        break;
      case OUTPUT_INT:
        s << "OUTPUT_INT";
        break;
      case INPUT_INT:
        s << "INPUT_INT";
        break;
      case OPEN_CHAN:
        s << "OPEN_CHAN";
        break;
      case CLOSE_CHAN:
        s << "CLOSE_CHAN";
        break;
      case STARTOPEN:
        s << "STARTOPEN";
        break;
      case STOPOPEN:
        s << "STOPOPEN";
        break;
      case TRIPLE:
        s << "TRIPLE";
        break;
      case BIT:
        s << "BIT";
        break;
      case SQUARE:
        s << "SQUARE";
        break;
      case ANDC:
        s << "ANDC";
        break;
      case XORC:
        s << "XORC";
        break;
      case ORC:
        s << "ORC";
        break;
      case ANDCI:
        s << "ANDCI";
        break;
      case XORCI:
        s << "XORCI";
        break;
      case ORCI:
        s << "ORCI";
        break;
      case NOTC:
        s << "NOTC";
        break;
      case SHLC:
        s << "SHLC";
        break;
      case SHRC:
        s << "SHLR";
        break;
      case SHLCI:
        s << "SHLCI";
        break;
      case SHRCI:
        s << "SHRCI";
        break;
      case JMP:
        s << "JMP";
        break;
      case JMPNZ:
        s << "JMPNZ";
        break;
      case JMPEQZ:
        s << "JMPEQZ";
        break;
      case EQZINT:
        s << "EQZINT";
        break;
      case LTZINT:
        s << "LTZINT";
        break;
      case LTINT:
        s << "LTINT";
        break;
      case GTINT:
        s << "GTINT";
        break;
      case EQINT:
        s << "EQINT";
        break;
      case JMPI:
        s << "JMPI";
        break;
      case LDINT:
        s << "LDINT";
        break;
      case ADDINT:
        s << "ADDINT";
        break;
      case SUBINT:
        s << "SUBINT";
        break;
      case MULINT:
        s << "MULINT";
        break;
      case DIVINT:
        s << "DIVINT";
        break;
      case CONVINT:
        s << "CONVINT";
        break;
      case CONVMODP:
        s << "CONVMODP";
        break;
      case PRINTMEM:
        s << "PRINTMEM";
        break;
      case PRINTREG:
        s << "PRINTREG";
        break;
      case PRINTREGPLAIN:
        s << "PRINTREGPLAIN";
        break;
      case PRINTCHR:
        s << "PRINTCHR";
        break;
      case PRINTSTR:
        s << "PRINTSTR";
        break;
      case PRINTCHRINT:
        s << "PRINTCHRINT";
        break;
      case PRINTSTRINT:
        s << "PRINTSTRINT";
        break;
      case PRINTFLOATPLAIN:
        s << "PRINTFLOATPLAIN";
        break;
      case PRINTFIXPLAIN:
        s << "PRINTFIXPLAIN";
        break;
      case PRINTINT:
        s << "PRINTINT";
        break;
      case RAND:
        s << "RAND";
        break;
      case START_TIMER:
        s << "START_TIMER";
        break;
      case STOP_TIMER:
        s << "STOP_TIMER";
        break;
      case LDMSINT:
        s << "LDMSINT";
        break;
      case LDMSINTI:
        s << "LDMSINTI";
        break;
      case STMSINT:
        s << "STMSINT";
        break;
      case STMSINTI:
        s << "STMSINTI";
        break;
      case MOVSINT:
        s << "MOVSINT";
        break;
      case LDSINT:
        s << "LDSINT";
        break;
      case ADDSINT:
        s << "ADDSINT";
        break;
      case ADDSINTC:
        s << "ADDSINTC";
        break;
      case SUBSINT:
        s << "SUBSINT";
        break;
      case SUBSINTC:
        s << "SUBSINTC";
        break;
      case SUBCINTS:
        s << "SUBCINTS";
        break;
      case MULSINT:
        s << "MULSINT";
        break;
      case MULSINTC:
        s << "MULSINTC";
        break;
      case DIVSINT:
        s << "DIVSINT";
        break;
      case SHLSINT:
        s << "SHLSINT";
        break;
      case SHRSINT:
        s << "SHRSINT";
        break;
      case NEG:
        s << "NEG";
        break;
      case SAND:
        s << "SAND";
        break;
      case XORSB:
        s << "XORSB";
        break;
      case ANDSB:
        s << "ANDSB";
        break;
      case ORSB:
        s << "ORSB";
        break;
      case NEGB:
        s << "NEGB";
        break;
      case LTZSINT:
        s << "LTZSINT";
        break;
      case EQZSINT:
        s << "EQZSINT";
        break;
      case BITSINT:
        s << "BITSINT";
        break;
      case SINTBIT:
        s << "SINTBIT";
        break;
      case CONVSINTSREG:
        s << "CONVSINTSREG";
        break;
      case CONVREGSREG:
        s << "CONVREGSREG";
        break;
      case CONVSREGSINT:
        s << "CONVSREGSINT";
        break;
      case OPENSINT:
        s << "OPENSINT";
        break;
      case OPENSBIT:
        s << "OPENSBIT";
        break;
      case ANDSINT:
        s << "ANDSINT";
        break;
      case ANDSINTC:
        s << "ANDSINTC";
        break;
      case ORSINT:
        s << "ORSINT";
        break;
      case ORSINTC:
        s << "ORSINTC";
        break;
      case XORSINT:
        s << "XORSINT";
        break;
      case XORSINTC:
        s << "XORSINTC";
        break;
      case INVSINT:
        s << "INVSINT";
        break;
      case MUL2SINT:
        s << "MUL2SINT";
        break;
      case GC:
        s << "GC";
        break;
      case LF_CINT:
        s << "LF_CINT";
        break;
      case LF_SINT:
        s << "LF_SINT";
        break;
      case LF_REGINT:
        s << "LF_REGINT";
        break;
      case LF_SREGINT:
        s << "LF_SREGINT";
        break;
      default:
        s << instr.opcode;
        throw Invalid_Instruction("Verbose Opcode Note Known");
    }

  if (instr.size != 1)
    {
      s << " " << instr.size;
    }
  s << "  ";

  // Now the arguments
  switch (instr.opcode)
    {
      // instructions with 3 cint register operands */
      case ADDC:
      case SUBC:
      case MULC:
      case DIVC:
      case MODC:
      case ANDC:
      case XORC:
      case ORC:
      case SHLC:
      case SHRC:
        s << "c_" << instr.r[0] << " ";
        s << "c_" << instr.r[1] << " ";
        s << "c_" << instr.r[2] << " ";
        break;
      // instructions with 3 sint register operands */
      case ADDS:
      case SUBS:
      case TRIPLE:
        s << "s_" << instr.r[0] << " ";
        s << "s_" << instr.r[1] << " ";
        s << "s_" << instr.r[2] << " ";
        break;
      // instructions with 3 rint register operands */
      case ADDINT:
      case SUBINT:
      case MULINT:
      case DIVINT:
      case LTINT:
      case GTINT:
      case EQINT:
        s << "r_" << instr.r[0] << " ";
        s << "r_" << instr.r[1] << " ";
        s << "r_" << instr.r[2] << " ";
        break;
      // instructions with 3 sregint register operands */
      case ADDSINT:
      case SUBSINT:
      case MULSINT:
      case DIVSINT:
      case ANDSINT:
      case ORSINT:
      case XORSINT:
        s << "sr_" << instr.r[0] << " ";
        s << "sr_" << instr.r[1] << " ";
        s << "sr_" << instr.r[2] << " ";
        break;
      // instructions with 3 sbit register operands */
      case XORSB:
      case ANDSB:
      case ORSB:
        s << "sb_" << instr.r[0] << " ";
        s << "sb_" << instr.r[1] << " ";
        s << "sb_" << instr.r[2] << " ";
        break;
      // instructions with 2 sint + 1 cint register operands */
      case ADDM:
      case SUBML:
      case MULM:
        s << "s_" << instr.r[0] << " ";
        s << "s_" << instr.r[1] << " ";
        s << "c_" << instr.r[2] << " ";
        break;
      // instructions with 2 sregint and 1 rint operand
      case ADDSINTC:
      case SUBSINTC:
      case MULSINTC:
      case ANDSINTC:
      case ORSINTC:
      case XORSINTC:
        s << "sr_" << instr.r[0] << " ";
        s << "sr_" << instr.r[1] << " ";
        s << "r_" << instr.r[2] << " ";
        break;
      // instructions with 2 sregint and 1 sbit operand
      case SAND:
        s << "sr_" << instr.r[0] << " ";
        s << "sr_" << instr.r[1] << " ";
        s << "sb_" << instr.r[2] << " ";
        break;
      // instructions with 1 sbit and 1 sregint operands
      case LTZSINT:
      case EQZSINT:
        s << "sb_" << instr.r[0] << " ";
        s << "sr_" << instr.r[1] << " ";
        break;
      // instructions with 1 sbit and 1 sregint and 1 integer operands
      case BITSINT:
        s << "sb_" << instr.r[0] << " ";
        s << "sr_" << instr.r[1] << " ";
        s << instr.n << " ";
        break;
      // instructions with 1 sregint and 1 sbit and 1 integer operands
      case SINTBIT:
        s << "sr_" << instr.r[0] << " ";
        s << "sr_" << instr.r[1] << " ";
        s << "sb_" << instr.r[2] << " ";
        s << instr.n << " ";
        break;
      // instructions with 1 sint + 1 cint + 1 sint register operands */
      case SUBMR:
        s << "s_" << instr.r[0] << " ";
        s << "c_" << instr.r[1] << " ";
        s << "s_" << instr.r[2] << " ";
        break;
      // instructions with 1 sregint + 1 rint + 1 sregint operand
      case SUBCINTS:
        s << "sr_" << instr.r[0] << " ";
        s << "r_" << instr.r[1] << " ";
        s << "sr_" << instr.r[2] << " ";
        break;
      // instructions with 1 cint + 1 rint register operand
      case LDMCI:
      case CONVINT:
      case STMCI:
        s << "c_" << instr.r[0] << " ";
        s << "r_" << instr.r[1] << " ";
        break;
      // instructions with 1 sint + 1 rint register operand
      case LDMSI:
      case STMSI:
        s << "s_" << instr.r[0] << " ";
        s << "r_" << instr.r[1] << " ";
        break;
      // instructions with 1 sregint + 1 rint register operand
      case LDMSINTI:
      case CONVREGSREG:
        s << "sr_" << instr.r[0] << " ";
        s << "r_" << instr.r[1] << " ";
        break;
      // instructions with 1 sregint + 1 sint register operand
      case CONVSINTSREG:
        s << "sr_" << instr.r[0] << " ";
        s << "s_" << instr.r[1] << " ";
        break;
      // instructions with 1 sint + 1 sregint register operand
      case CONVSREGSINT:
        s << "s_" << instr.r[0] << " ";
        s << "sr_" << instr.r[1] << " ";
        break;
      // instructions with 1 rint + 1 sregint register operand
      case OPENSINT:
        s << "r_" << instr.r[0] << " ";
        s << "sr_" << instr.r[1] << " ";
        break;
      // instructions with 1 rint + 1 sbit register operand
      case OPENSBIT:
        s << "r_" << instr.r[0] << " ";
        s << "sb_" << instr.r[1] << " ";
        break;
      // instructions with 2 cint register operands
      case MOVC:
      case LEGENDREC:
      case DIGESTC:
        s << "c_" << instr.r[0] << " ";
        s << "c_" << instr.r[1] << " ";
        break;
      // instructions with 2 sint register operands
      case MOVS:
      case SQUARE:
        s << "s_" << instr.r[0] << " ";
        s << "s_" << instr.r[1] << " ";
        break;
      // instructions with 2 rint register operands
      case MOVINT:
      case LDMINTI:
      case RAND:
      case STMINTI:
      case STMSINTI:
      case LTZINT:
      case EQZINT:
        s << "r_" << instr.r[0] << " ";
        s << "r_" << instr.r[1] << " ";
        break;
      // instructions with 2 sregint operands
      case NEG:
      case MOVSINT:
      case INVSINT:
        s << "sr_" << instr.r[0] << " ";
        s << "sr_" << instr.r[1] << " ";
        break;
      // instructions with 2 sbit operands
      case NEGB:
        s << "sb_" << instr.r[0] << " ";
        s << "sb_" << instr.r[1] << " ";
        break;
      // instructions with 1 cint register operands
      case PRINTREGPLAIN:
        s << "c_" << instr.r[0] << " ";
        break;
      // instructions with 1 sint register operands
      case BIT:
        s << "s_" << instr.r[0] << " ";
        break;
      // instructions with 1 rint register operands
      case PRINTINT:
      case PRINTCHRINT:
      case PRINTSTRINT:
      case JMPI:
      case PUSHINT:
      case POPINT:
      case STARG:
      case LDTN:
      case LDARG:
        s << "r_" << instr.r[0] << " ";
        break;
      // instructions with 2 cint + 1 integer operand
      case ADDCI:
      case SUBCI:
      case SUBCFI:
      case MULCI:
      case DIVCI:
      case MODCI:
      case ANDCI:
      case XORCI:
      case ORCI:
      case SHLCI:
      case NOTC:
        s << "c_" << instr.r[0] << " ";
        s << "c_" << instr.r[1] << " ";
        s << instr.n << " ";
        break;
      // instructions with 2 sint + 1 integer operand
      case ADDSI:
      case SUBSI:
      case SUBSFI:
      case MULSI:
      case SHRCI:
        s << "s_" << instr.r[0] << " ";
        s << "s_" << instr.r[1] << " ";
        s << instr.n << " ";
        break;
      // instructions with 2 sregint + 1 integer operand
      case SHLSINT:
      case SHRSINT:
        s << "sr_" << instr.r[0] << " ";
        s << "sr_" << instr.r[1] << " ";
        s << instr.n << " ";
        break;
      // instructions with 1 rint + 1 cint + 1 integer operand
      case CONVMODP:
        s << "r_" << instr.r[0] << " ";
        s << "c_" << instr.r[1] << " ";
        s << instr.n << " ";
        break;
      // instructions with 1 rint + 1 integer operand
      case LDINT:
      case LDMINT:
      case STMINT:
      case JMPNZ:
      case JMPEQZ:
      case INPUT_INT:
      case OPEN_CHAN:
      case OUTPUT_INT:
        s << "r_" << instr.r[0] << " ";
        s << instr.n << " ";
        break;
      // instructions with 1 sint + 1 integer operand
      case LDSI:
      case LDMS:
      case STMS:
        s << "s_" << instr.r[0] << " ";
        s << instr.n << " ";
        break;
      // instructions with 1 cint + 1 integer operand
      case LDI:
      case LDMC:
      case STMC:
      case PRINTREG:
      case INPUT_CLEAR:
      case OUTPUT_CLEAR:
        s << "c_" << instr.r[0] << " ";
        s << instr.n << " ";
        break;
      // instructions with 1 sregint + 1 integer operand
      case LDSINT:
      case LDMSINT:
      case STMSINT:
        s << "sr_" << instr.r[0] << " ";
        s << instr.n << " ";
        break;
      // instructions with 1 sint + 1 player + 1 integer operand
      case PRIVATE_INPUT:
      case PRIVATE_OUTPUT:
        s << "s_" << instr.r[0] << " ";
        s << instr.p << " ";
        s << instr.m << " ";
        break;
      // instructions with 1 sint + 2 integer operands
      case PRINTFIXPLAIN:
        s << "c_" << instr.r[0] << " ";
        s << instr.n << " ";
        s << instr.m << " ";
        break;
      // instructions with 1 integer operand
      case PRINTSTR:
      case PRINTCHR:
      case REQBL:
      case JMP:
      case START_TIMER:
      case STOP_TIMER:
      case CLOSE_CHAN:
      case PRINTMEM:
      case JOIN_TAPE:
        s << instr.n << " ";
        break;
      // instructions with no operand
      case RESTART:
      case CRASH:
      case CLEAR_MEMORY:
      case CLEAR_REGISTERS:
        break;
      // Three integer operands
      case RUN_TAPE:
        s << instr.r[0] << " ";
        s << instr.r[1] << " ";
        s << instr.r[2] << " ";
        break;
      // Various variable length instructions
      case MUL2SINT:
        for (unsigned int i= 0; i < instr.start.size(); i++)
          {
            s << "sr_" << instr.start[i] << " ";
          }
        break;
      case PRINTFLOATPLAIN:
      case STOPOPEN:
        for (unsigned int i= 0; i < instr.start.size(); i++)
          {
            s << "c_" << instr.start[i] << " ";
          }
        break;
      case STARTOPEN:
        for (unsigned int i= 0; i < instr.start.size(); i++)
          {
            s << "s_" << instr.start[i] << " ";
          }
        break;
      case GC:
        s << instr.start[0] << " " << instr.start[1] << " " << instr.start[2] << " ";
        for (unsigned int i= 3; i < instr.start.size(); i++)
          {
            s << "sr_" << instr.start[i] << " ";
          }
        break;
      case LF_SINT:
        for (unsigned int i= 0; i < 6; i++)
          {
            s << instr.start[i] << " ";
          }
        for (int i= 6; i < 6 + instr.start[1]; i++)
          {
            s << "s_" << instr.start[i] << " ";
          }
        break;
      case LF_CINT:
        for (unsigned int i= 0; i < 6; i++)
          {
            s << instr.start[i] << " ";
          }
        for (int i= 6; i < 6 + instr.start[1]; i++)
          {
            s << "c_" << instr.start[i] << " ";
          }
        break;
      case LF_REGINT:
        for (unsigned int i= 0; i < 6; i++)
          {
            s << instr.start[i] << " ";
          }
        for (int i= 6; i < 6 + instr.start[1]; i++)
          {
            s << "r_" << instr.start[i] << " ";
          }
        break;
      case LF_SREGINT:
        for (unsigned int i= 0; i < 6; i++)
          {
            s << instr.start[i] << " ";
          }
        for (int i= 6; i < 6 + instr.start[1]; i++)
          {
            s << "sr_" << instr.start[i] << " ";
          }
        break;
      case OUTPUT_SHARE:
      case INPUT_SHARE:
        s << instr.p << " ";
        for (unsigned int i= 0; i < instr.start.size(); i++)
          {
            s << "s_" << instr.start[i] << " ";
          }
        break;
      default:
        throw Invalid_Instruction("Cannot parse operarands in verbose mode");
    }

  // Finish the data for the LF_* instructions
  switch (instr.opcode)
    {
      case LF_SINT:
      case LF_CINT:
      case LF_REGINT:
      case LF_SREGINT:
        int i;
        for (i= 6 + instr.start[1]; i < 6 + instr.start[1] + instr.start[2]; i++)
          {
            s << "r_" << instr.start[i] << " ";
          }
        for (; i < 6 + instr.start[1] + instr.start[2] + instr.start[3]; i++)
          {
            s << "sr_" << instr.start[i] << " ";
          }
        for (; i < 6 + instr.start[1] + instr.start[2] + instr.start[3] + instr.start[4]; i++)
          {
            s << "c_" << instr.start[i] << " ";
          }
        for (; i < 6 + instr.start[1] + instr.start[2] + instr.start[3] + instr.start[4] + instr.start[5]; i++)
          {
            s << "s_" << instr.start[i] << " ";
          }
      default:
        break;
    }
  return s;
}

void Instruction::execute_using_sacrifice_data(
    Processor &Proc, offline_control_data &OCD) const
{
  int thread= Proc.get_thread_num();
  // Check to see if we have to wait
  Wait_For_Preproc(opcode, size, thread, OCD);
  // Now do the work
  OCD.sacrifice_mutex[thread].lock();
  Proc.increment_PC();

  int r[3]= {this->r[0], this->r[1], this->r[2]};

  for (unsigned int i= 0; i < size; i++)
    {
      switch (opcode)
        {
          case TRIPLE:
            Proc.get_Sp_ref(r[0])= SacrificeD[thread].TD.ta.front();
            SacrificeD[thread].TD.ta.pop_front();
            Proc.get_Sp_ref(r[1])= SacrificeD[thread].TD.tb.front();
            SacrificeD[thread].TD.tb.pop_front();
            Proc.get_Sp_ref(r[2])= SacrificeD[thread].TD.tc.front();
            SacrificeD[thread].TD.tc.pop_front();
            break;
          case SQUARE:
            Proc.get_Sp_ref(r[0])= SacrificeD[thread].SD.sa.front();
            SacrificeD[thread].SD.sa.pop_front();
            Proc.get_Sp_ref(r[1])= SacrificeD[thread].SD.sb.front();
            SacrificeD[thread].SD.sb.pop_front();
            break;
          case BIT:
            Proc.get_Sp_ref(r[0])= SacrificeD[thread].BD.bb.front();
            SacrificeD[thread].BD.bb.pop_front();
            break;
          default:
            throw bad_value();
            break;
        }
      if (size > 1)
        {
          r[0]++;
          r[1]++;
          r[2]++;
        }
    }
  OCD.sacrifice_mutex[thread].unlock();
}

bool Instruction::execute(Processor &Proc, Player &P, Machine &machine,
                          offline_control_data &OCD) const
{
  if (machine.verbose)
    {
      stringstream s;
      s << *this;
      printf("Thread %d : %s\n", Proc.get_thread_num(), s.str().c_str());
      fflush(stdout);
    }
  bool restart= false;

  // First deal with the offline data input routines as these need thread locking
  if (opcode == TRIPLE || opcode == SQUARE || opcode == BIT)
    {
      execute_using_sacrifice_data(Proc, OCD);
      return restart;
    }

  Proc.increment_PC();

  int r[3]= {this->r[0], this->r[1], this->r[2]};
  int n= this->n;

  for (unsigned int i= 0; i < size; i++)
    {
      switch (opcode)
        {
          case LDI:
            Proc.temp.ansp.assign(n);
            Proc.write_Cp(r[0], Proc.temp.ansp);
            break;
          case LDSI:
            {
              Proc.temp.ansp.assign(n);
              Proc.get_Sp_ref(r[0]).assign(Proc.temp.ansp, P.get_mac_keys());
            }
            break;
          case LDMC:
            Proc.write_Cp(r[0], machine.Mc.read(n));
            n++;
            break;
          case LDMS:
            Proc.write_Sp(r[0], machine.Ms.read(n));
            n++;
            break;
          case LDMINT:
            Proc.write_Ri(r[0], machine.Mr.read(n).get());
            n++;
            break;
          case LDMCI:
            Proc.write_Cp(r[0], machine.Mc.read(Proc.read_Ri(r[1])));
            break;
          case LDMSI:
            Proc.write_Sp(r[0], machine.Ms.read(Proc.read_Ri(r[1])));
            break;
          case LDMINTI:
            Proc.write_Ri(r[0], machine.Mr.read(Proc.read_Ri(r[1])).get());
            break;
          case STMC:
            machine.Mc.write(n, Proc.read_Cp(r[0]), Proc.get_PC());
            n++;
            break;
          case STMS:
            machine.Ms.write(n, Proc.read_Sp(r[0]), Proc.get_PC());
            n++;
            break;
          case STMINT:
            machine.Mr.write(n, Integer(Proc.read_Ri(r[0])), Proc.get_PC());
            n++;
            break;
          case STMCI:
            machine.Mc.write(Proc.read_Ri(r[1]), Proc.read_Cp(r[0]), Proc.get_PC());
            break;
          case STMSI:
            machine.Ms.write(Proc.read_Ri(r[1]), Proc.read_Sp(r[0]), Proc.get_PC());
            break;
          case STMINTI:
            machine.Mr.write(Proc.read_Ri(r[1]), Integer(Proc.read_Ri(r[0])),
                             Proc.get_PC());
            break;
          case MOVC:
            Proc.write_Cp(r[0], Proc.read_Cp(r[1]));
            break;
          case MOVS:
            Proc.write_Sp(r[0], Proc.read_Sp(r[1]));
            break;
          case MOVINT:
            Proc.write_Ri(r[0], Proc.read_Ri(r[1]));
            break;
          case PUSHINT:
            Proc.pushi(Proc.read_Ri(r[0]));
            break;
          case POPINT:
            Proc.popi(Proc.get_Ri_ref(r[0]));
            break;
          case LDTN:
            Proc.write_Ri(r[0], Proc.get_thread_num());
            break;
          case LDARG:
            Proc.write_Ri(r[0], Proc.get_arg());
            break;
          case STARG:
            Proc.set_arg(Proc.read_Ri(r[0]));
            break;
          case ADDC:
#ifdef DEBUG
            Proc.temp.ansp.add(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).add(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
#endif
            break;
          case ADDS:
#ifdef DEBUG
            Proc.temp.Sansp.add(Proc.read_Sp(r[1]), Proc.read_Sp(r[2]));
            Proc.write_Sp(r[0], Proc.temp.Sansp);
#else
            Proc.get_Sp_ref(r[0]).add(Proc.read_Sp(r[1]), Proc.read_Sp(r[2]));
#endif
            break;
          case ADDM:
#ifdef DEBUG
            Proc.temp.Sansp.add(Proc.read_Sp(r[1]), Proc.read_Cp(r[2]), P.get_mac_keys());
            Proc.write_Sp(r[0], Proc.temp.Sansp);
#else
            Proc.get_Sp_ref(r[0]).add(Proc.read_Sp(r[1]), Proc.read_Cp(r[2]),
                                      P.get_mac_keys());
#endif
            break;
          case SUBC:
#ifdef DEBUG
            Proc.temp.ansp.sub(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).sub(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
#endif
            break;
          case SUBS:
#ifdef DEBUG
            Proc.temp.Sansp.sub(Proc.read_Sp(r[1]), Proc.read_Sp(r[2]));
            Proc.write_Sp(r[0], Proc.temp.Sansp);
#else
            Proc.get_Sp_ref(r[0]).sub(Proc.read_Sp(r[1]), Proc.read_Sp(r[2]));
#endif
            break;
          case SUBML:
#ifdef DEBUG
            Proc.temp.Sansp.sub(Proc.read_Sp(r[1]), Proc.read_Cp(r[2]), P.get_mac_keys());
            Proc.write_Sp(r[0], Proc.temp.Sansp);
#else
            Proc.get_Sp_ref(r[0]).sub(Proc.read_Sp(r[1]), Proc.read_Cp(r[2]),
                                      P.get_mac_keys());
#endif
            break;
          case SUBMR:
#ifdef DEBUG
            Proc.temp.Sansp.sub(Proc.read_Cp(r[1]), Proc.read_Sp(r[2]), P.get_mac_keys());
            Proc.write_Sp(r[0], Proc.temp.Sansp);
#else
            Proc.get_Sp_ref(r[0]).sub(Proc.read_Cp(r[1]), Proc.read_Sp(r[2]),
                                      P.get_mac_keys());
#endif
            break;
          case MULC:
#ifdef DEBUG
            Proc.temp.ansp.mul(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).mul(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
#endif
            break;
          case MULM:
#ifdef DEBUG
            Proc.temp.Sansp.mul(Proc.read_Sp(r[1]), Proc.read_Cp(r[2]));
            Proc.write_Sp(r[0], Proc.temp.Sansp);
#else
            Proc.get_Sp_ref(r[0]).mul(Proc.read_Sp(r[1]), Proc.read_Cp(r[2]));
#endif
            break;
          case DIVC:
            if (Proc.read_Cp(r[2]).is_zero())
              throw Processor_Error("Division by zero from register");
            Proc.temp.ansp.invert(Proc.read_Cp(r[2]));
            Proc.temp.ansp.mul(Proc.read_Cp(r[1]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
            break;
          case MODC:
            to_bigint(Proc.temp.aa, Proc.read_Cp(r[1]));
            to_bigint(Proc.temp.aa2, Proc.read_Cp(r[2]));
            mpz_fdiv_r(Proc.temp.aa.get_mpz_t(), Proc.temp.aa.get_mpz_t(),
                       Proc.temp.aa2.get_mpz_t());
            to_gfp(Proc.temp.ansp, Proc.temp.aa);
            Proc.write_Cp(r[0], Proc.temp.ansp);
            break;
          case LEGENDREC:
            to_bigint(Proc.temp.aa, Proc.read_Cp(r[1]));
            Proc.temp.aa=
                mpz_legendre(Proc.temp.aa.get_mpz_t(), gfp::pr().get_mpz_t());
            to_gfp(Proc.temp.ansp, Proc.temp.aa);
            Proc.write_Cp(r[0], Proc.temp.ansp);
            break;
          case DIGESTC:
            {
              stringstream o;
              to_bigint(Proc.temp.aa, Proc.read_Cp(r[1]));

              to_gfp(Proc.temp.ansp, Proc.temp.aa);
              Proc.temp.ansp.output(o, false);
              string s= Hash(o.str());
              // keep first n bytes
              istringstream is(s);
              Proc.temp.ansp.input(is, false);
              Proc.write_Cp(r[0], Proc.temp.ansp);
            }
            break;
          case DIVCI:
            if (n == 0)
              throw Processor_Error("Division by immediate zero");
            to_gfp(Proc.temp.ansp, n % gfp::pr());
            Proc.temp.ansp.invert();
            Proc.temp.ansp.mul(Proc.read_Cp(r[1]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
            break;
          case MODCI:
            to_bigint(Proc.temp.aa, Proc.read_Cp(r[1]));
            to_gfp(Proc.temp.ansp, mpz_fdiv_ui(Proc.temp.aa.get_mpz_t(), n));
            Proc.write_Cp(r[0], Proc.temp.ansp);
            break;
          case ADDCI:
            Proc.temp.ansp.assign(n);
#ifdef DEBUG
            Proc.temp.ansp.add(Proc.temp.ansp, Proc.read_Cp(r[1]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).add(Proc.temp.ansp, Proc.read_Cp(r[1]));
#endif
            break;
          case ADDSI:
            Proc.temp.ansp.assign(n);
#ifdef DEBUG
            Proc.temp.Sansp.add(Proc.read_Sp(r[1]), Proc.temp.ansp, P.get_mac_keys());
            Proc.write_Sp(r[0], Proc.temp.Sansp);
#else
            Proc.get_Sp_ref(r[0]).add(Proc.read_Sp(r[1]), Proc.temp.ansp,
                                      P.get_mac_keys());
#endif
            break;
          case SUBCI:
            Proc.temp.ansp.assign(n);
#ifdef DEBUG
            Proc.temp.ansp.sub(Proc.read_Cp(r[1]), Proc.temp.ansp);
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).sub(Proc.read_Cp(r[1]), Proc.temp.ansp);
#endif
            break;
          case SUBSI:
            Proc.temp.ansp.assign(n);
#ifdef DEBUG
            Proc.temp.Sansp.sub(Proc.read_Sp(r[1]), Proc.temp.ansp, P.get_mac_keys());
            Proc.write_Sp(r[0], Proc.temp.Sansp);
#else
            Proc.get_Sp_ref(r[0]).sub(Proc.read_Sp(r[1]), Proc.temp.ansp,
                                      P.get_mac_keys());
#endif
            break;
          case SUBCFI:
            Proc.temp.ansp.assign(n);
#ifdef DEBUG
            Proc.temp.ansp.sub(Proc.temp.ansp, Proc.read_Cp(r[1]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).sub(Proc.temp.ansp, Proc.read_Cp(r[1]));
#endif
            break;
          case SUBSFI:
            Proc.temp.ansp.assign(n);
#ifdef DEBUG
            Proc.temp.Sansp.sub(Proc.temp.ansp, Proc.read_Sp(r[1]), P.get_mac_keys());
            Proc.write_Sp(r[0], Proc.temp.Sansp);
#else
            Proc.get_Sp_ref(r[0]).sub(Proc.temp.ansp, Proc.read_Sp(r[1]),
                                      P.get_mac_keys());
#endif
            break;
          case MULCI:
            Proc.temp.ansp.assign(n);
#ifdef DEBUG
            Proc.temp.ansp.mul(Proc.temp.ansp, Proc.read_Cp(r[1]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).mul(Proc.temp.ansp, Proc.read_Cp(r[1]));
#endif
            break;
          case MULSI:
            Proc.temp.ansp.assign(n);
#ifdef DEBUG
            Proc.temp.Sansp.mul(Proc.read_Sp(r[1]), Proc.temp.ansp);
            Proc.write_Sp(r[0], Proc.temp.Sansp);
#else
            Proc.get_Sp_ref(r[0]).mul(Proc.read_Sp(r[1]), Proc.temp.ansp);
#endif
            break;
          case ANDC:
#ifdef DEBUG
            Proc.temp.ansp.AND(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).AND(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
#endif
            break;
          case XORC:
#ifdef DEBUG
            Proc.temp.ansp.XOR(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).XOR(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
#endif
            break;
          case ORC:
#ifdef DEBUG
            Proc.temp.ansp.OR(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).OR(Proc.read_Cp(r[1]), Proc.read_Cp(r[2]));
#endif
            break;
          case ANDCI:
            Proc.temp.aa= n;
#ifdef DEBUG
            Proc.temp.ansp.AND(Proc.read_Cp(r[1]), Proc.temp.aa);
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).AND(Proc.read_Cp(r[1]), Proc.temp.aa);
#endif
            break;
          case XORCI:
            Proc.temp.aa= n;
#ifdef DEBUG
            Proc.temp.ansp.XOR(Proc.read_Cp(r[1]), Proc.temp.aa);
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).XOR(Proc.read_Cp(r[1]), Proc.temp.aa);
#endif
            break;
          case ORCI:
            Proc.temp.aa= n;
#ifdef DEBUG
            Proc.temp.ansp.OR(Proc.read_Cp(r[1]), Proc.temp.aa);
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).OR(Proc.read_Cp(r[1]), Proc.temp.aa);
#endif
            break;
          case NOTC:
            to_bigint(Proc.temp.aa, Proc.read_Cp(r[1]));
            mpz_com(Proc.temp.aa.get_mpz_t(), Proc.temp.aa.get_mpz_t());
            Proc.temp.aa2= 1;
            Proc.temp.aa2<<= n;
            Proc.temp.aa+= Proc.temp.aa2;
            to_gfp(Proc.temp.ansp, Proc.temp.aa);
            Proc.write_Cp(r[0], Proc.temp.ansp);
            break;
          case SHLC:
            to_bigint(Proc.temp.aa, Proc.read_Cp(r[2]));
            if (Proc.temp.aa > 63)
              throw not_implemented();
#ifdef DEBUG
            Proc.temp.ansp.SHL(Proc.read_Cp(r[1]), Proc.temp.aa);
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).SHL(Proc.read_Cp(r[1]), Proc.temp.aa);
#endif
            break;
          case SHRC:
            to_bigint(Proc.temp.aa, Proc.read_Cp(r[2]));
            if (Proc.temp.aa > 63)
              throw not_implemented();
#ifdef DEBUG
            Proc.temp.ansp.SHR(Proc.read_Cp(r[1]), Proc.temp.aa);
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).SHR(Proc.read_Cp(r[1]), Proc.temp.aa);
#endif
            break;
          case SHLCI:
#ifdef DEBUG
            Proc.temp.ansp.SHL(Proc.read_Cp(r[1]), Proc.temp.aa);
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).SHL(Proc.read_Cp(r[1]), n);
#endif
            break;
          case SHRCI:
#ifdef DEBUG
            Proc.temp.ansp.SHR(Proc.read_Cp(r[1]), Proc.temp.aa);
            Proc.write_Cp(r[0], Proc.temp.ansp);
#else
            Proc.get_Cp_ref(r[0]).SHR(Proc.read_Cp(r[1]), n);
#endif
            break;
          case STARTOPEN:
            Proc.POpen_Start(start, size, P);
            return restart;
          case STOPOPEN:
            Proc.POpen_Stop(start, size, P);
            return restart;
          case JMP:
            Proc.relative_jump((signed int) n);
            break;
          case JMPI:
            Proc.relative_jump((signed int) Proc.read_Ri(r[0]));
            break;
          case JMPNZ:
            if (Proc.read_Ri(r[0]) != 0)
              {
                Proc.relative_jump((signed int) n);
              }
            break;
          case JMPEQZ:
            if (Proc.read_Ri(r[0]) == 0)
              {
                Proc.relative_jump((signed int) n);
              }
            break;
          case EQZINT:
            if (Proc.read_Ri(r[1]) == 0)
              Proc.write_Ri(r[0], 1);
            else
              Proc.write_Ri(r[0], 0);
            break;
          case LTZINT:
            if (Proc.read_Ri(r[1]) < 0)
              Proc.write_Ri(r[0], 1);
            else
              Proc.write_Ri(r[0], 0);
            break;
          case LTINT:
            if (Proc.read_Ri(r[1]) < Proc.read_Ri(r[2]))
              Proc.write_Ri(r[0], 1);
            else
              Proc.write_Ri(r[0], 0);
            break;
          case GTINT:
            if (Proc.read_Ri(r[1]) > Proc.read_Ri(r[2]))
              Proc.write_Ri(r[0], 1);
            else
              Proc.write_Ri(r[0], 0);
            break;
          case EQINT:
            if (Proc.read_Ri(r[1]) == Proc.read_Ri(r[2]))
              Proc.write_Ri(r[0], 1);
            else
              Proc.write_Ri(r[0], 0);
            break;
          case LDINT:
            Proc.write_Ri(r[0], n);
            break;
          case ADDINT:
            Proc.get_Ri_ref(r[0])= Proc.read_Ri(r[1]) + Proc.read_Ri(r[2]);
            break;
          case SUBINT:
            Proc.get_Ri_ref(r[0])= Proc.read_Ri(r[1]) - Proc.read_Ri(r[2]);
            break;
          case MULINT:
            Proc.get_Ri_ref(r[0])= Proc.read_Ri(r[1]) * Proc.read_Ri(r[2]);
            break;
          case DIVINT:
            Proc.get_Ri_ref(r[0])= Proc.read_Ri(r[1]) / Proc.read_Ri(r[2]);
            break;
          case CONVINT:
            Proc.get_Cp_ref(r[0]).assign(Proc.read_Ri(r[1]));
            break;
          case CONVMODP:
            to_signed_bigint(Proc.temp.aa, Proc.read_Cp(r[1]), n);
            Proc.write_Ri(r[0], Proc.temp.aa.get_si());
            break;
          case PRINTMEM:
            if (P.whoami() == 0)
              {
                stringstream ss;
                ss << "Mem[" << n << "] = " << machine.Mc.read(n) << endl;
                machine.get_IO().debug_output(ss);
              }
            break;
          case PRINTREG:
            if (P.whoami() == 0)
              {
                stringstream ss;
                ss << "Reg[" << r[0] << "] = " << Proc.read_Cp(r[0]) << " # "
                   << string((char *) &n, sizeof(n)) << endl;
                machine.get_IO().debug_output(ss);
              }
            break;
          case PRINTREGPLAIN:
            if (P.whoami() == 0)
              {
                stringstream ss;
                ss << Proc.read_Cp(r[0]);
                machine.get_IO().debug_output(ss);
              }
            break;
          case PRINTFIXPLAIN:
            if (P.whoami() == 0)
              {
                gfp v= Proc.read_Cp(r[0]);
                // immediate values
                auto f= n;
                auto k= m;
                // v has k bits max
                to_signed_bigint(Proc.temp.aa, v, k);
                mpf_class res= Proc.temp.aa;
                // compute v * 2^{-f}
                mpf_div_2exp(res.get_mpf_t(), res.get_mpf_t(), f);
                stringstream ss;
                ss << res;
                machine.get_IO().debug_output(ss);
              }
            break;
          case PRINTFLOATPLAIN:
            if (P.whoami() == 0)
              {
                gfp v= Proc.read_Cp(start[0]);
                gfp p= Proc.read_Cp(start[1]);
                gfp z= Proc.read_Cp(start[2]);
                gfp s= Proc.read_Cp(start[3]);
                to_bigint(Proc.temp.aa, v);
                // MPIR can't handle more precision in exponent
                to_signed_bigint(Proc.temp.aa2, p, 31);
                long exp= Proc.temp.aa2.get_si();
                mpf_class res= Proc.temp.aa;
                if (exp > 0)
                  mpf_mul_2exp(res.get_mpf_t(), res.get_mpf_t(), exp);
                else
                  mpf_div_2exp(res.get_mpf_t(), res.get_mpf_t(), -exp);
                if (z.is_one())
                  res= 0;
                if (!s.is_zero())
                  res*= -1;
                if (not z.is_bit() or not s.is_bit())
                  throw Processor_Error("invalid floating point number");
                stringstream ss;
                ss << res;
                machine.get_IO().debug_output(ss);
              }
            break;
          case PRINTINT:
            if (P.whoami() == 0)
              {
                stringstream ss;
                ss << Proc.read_Ri(r[0]);
                machine.get_IO().debug_output(ss);
              }
            break;
          case PRINTSTR:
            if (P.whoami() == 0)
              {
                stringstream ss;
                ss << string((char *) &n, sizeof(n));
                machine.get_IO().debug_output(ss);
              }
            break;
          case PRINTCHR:
            if (P.whoami() == 0)
              {
                stringstream ss;
                ss << string((char *) &n, 1);
                machine.get_IO().debug_output(ss);
              }
            break;
          case PRINTCHRINT:
            if (P.whoami() == 0)
              {
                stringstream ss;
                ss << string((char *) &(Proc.read_Ri(r[0])), 1);
                machine.get_IO().debug_output(ss);
              }
            break;
          case PRINTSTRINT:
            if (P.whoami() == 0)
              {
                stringstream ss;
                ss << string((char *) &(Proc.read_Ri(r[0])), sizeof(int));
                machine.get_IO().debug_output(ss);
              }
            break;
          case RAND:
            Proc.write_Ri(r[0], Proc.get_random_uint() % (1 << Proc.read_Ri(r[1])));
            break;
          case START_TIMER:
            machine.start_timer(n);
            break;
          case STOP_TIMER:
            machine.stop_timer(n);
            break;
          case REQBL:
            break;
          case RUN_TAPE:
            machine.run_tape(r[0], r[2], r[1]);
            break;
          case JOIN_TAPE:
            machine.Lock_Until_Finished_Tape(n);
            break;
          case CRASH:
            machine.get_IO().crash(Proc.get_PC() - 1, Proc.get_thread_num());
            // Note deliberately no "break" to enable CRASH to call RESTART
            // if the IO.crash returns
          case RESTART:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            restart= true;
            break;
          case CLEAR_MEMORY:
            machine.Mc.clear_memory();
            machine.Ms.clear_memory();
            machine.Mr.clear_memory();
            machine.Msr.clear_memory();
            break;
          case CLEAR_REGISTERS:
            Proc.clear_registers();
            break;
          case OUTPUT_SHARE:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            for (unsigned int i= 0; i < start.size(); i++)
              {
                machine.get_IO().output_share(Proc.get_Sp_ref(start[i]), p);
              }
            break;
          case INPUT_SHARE:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            for (unsigned int i= 0; i < start.size(); i++)
              {
                Proc.get_Sp_ref(start[i])= machine.get_IO().input_share(p);
              }
            break;
          case INPUT_CLEAR:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            Proc.get_Cp_ref(r[0])= machine.get_IO().public_input_gfp(n);
            break;
          case INPUT_INT:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            Proc.get_Ri_ref(r[0])= machine.get_IO().public_input_int(n);
            break;
          case OUTPUT_CLEAR:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            machine.get_IO().public_output_gfp(Proc.read_Cp(r[0]), n);
            break;
          case OUTPUT_INT:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            machine.get_IO().public_output_int(Proc.read_Ri(r[0]), n);
            break;
          case OPEN_CHAN:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            Proc.get_Ri_ref(r[0])= machine.get_IO().open_channel(n);
            break;
          case CLOSE_CHAN:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            machine.get_IO().close_channel(n);
            break;
          case PRIVATE_OUTPUT:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            Proc.iop.private_output(p, r[0], m, Proc, P, machine, OCD);
            break;
          case PRIVATE_INPUT:
            if (Proc.get_thread_num() != 0)
              {
                throw IO_thread();
              }
            Proc.iop.private_input(p, r[0], m, Proc, P, machine, OCD);
            break;
          /* Now we add in the new instructions for sregint and sbit operations */
          case LDMSINT:
            Proc.write_srint(r[0], machine.Msr.read(n));
            n++;
            break;
          case LDMSINTI:
            Proc.write_srint(r[0], machine.Msr.read(Proc.read_Ri(r[1])));
            break;
          case STMSINT:
            machine.Msr.write(n, Proc.read_srint(r[0]), Proc.get_PC());
            n++;
            break;
          case STMSINTI:
            machine.Msr.write(Proc.read_Ri(r[1]), Proc.read_srint(r[0]), Proc.get_PC());
            break;
          case MOVSINT:
            Proc.write_srint(r[0], Proc.read_srint(r[1]));
            break;
          case LDSINT:
            Proc.write_srint(r[0], n);
            break;
          case ADDSINT:
            Proc.get_srint_ref(r[0]).add(Proc.read_srint(r[1]), Proc.read_srint(r[2]), P, Proc.online_thread_num);
            break;
          case ADDSINTC:
            Proc.get_srint_ref(r[0]).add(Proc.read_srint(r[1]), Proc.read_Ri(r[2]), P, Proc.online_thread_num);
            break;
          case SUBSINT:
            Proc.get_srint_ref(r[0]).sub(Proc.read_srint(r[1]), Proc.read_srint(r[2]), P, Proc.online_thread_num);
            break;
          case SUBSINTC:
            Proc.get_srint_ref(r[0]).sub(Proc.read_srint(r[1]), Proc.read_Ri(r[2]), P, Proc.online_thread_num);
            break;
          case SUBCINTS:
            Proc.get_srint_ref(r[0]).sub(Proc.read_Ri(r[1]), Proc.read_srint(r[2]), P, Proc.online_thread_num);
            break;
          case MULSINT:
            Proc.get_srint_ref(r[0]).mul(Proc.read_srint(r[1]), Proc.read_srint(r[2]), P, Proc.online_thread_num);
            break;
          case MULSINTC:
            Proc.get_srint_ref(r[0]).mul(Proc.read_srint(r[1]), Proc.read_Ri(r[2]), P, Proc.online_thread_num);
            break;
          case MUL2SINT:
            mul(Proc.get_srint_ref(start[0]), Proc.get_srint_ref(start[1]), Proc.read_srint(start[2]), Proc.read_srint(start[3]), P, Proc.online_thread_num);
            break;
          case DIVSINT:
            Proc.get_srint_ref(r[0]).div(Proc.read_srint(r[1]), Proc.read_srint(r[2]), P, Proc.online_thread_num);
            break;
          case SHLSINT:
            Proc.get_srint_ref(r[0]).SHL(Proc.read_srint(r[1]), n);
            break;
          case SHRSINT:
            Proc.get_srint_ref(r[0]).SHR(Proc.read_srint(r[1]), n);
            break;
          case NEG:
            Proc.get_srint_ref(r[0]).negate(Proc.read_srint(r[1]), P, Proc.online_thread_num);
            break;
          case SAND:
            Proc.get_srint_ref(r[0]).Bit_AND(Proc.read_srint(r[1]), Proc.read_sbit(r[2]), P, Proc.online_thread_num);
            break;
          case ANDSINT:
            Proc.get_srint_ref(r[0]).Bitwise_AND(Proc.read_srint(r[1]), Proc.read_srint(r[2]), P, Proc.online_thread_num);
            break;
          case ANDSINTC:
            Proc.get_srint_ref(r[0]).Bitwise_AND(Proc.read_srint(r[1]), Proc.read_Ri(r[2]));
            break;
          case ORSINT:
            Proc.get_srint_ref(r[0]).Bitwise_OR(Proc.read_srint(r[1]), Proc.read_srint(r[2]), P, Proc.online_thread_num);
            break;
          case ORSINTC:
            Proc.get_srint_ref(r[0]).Bitwise_OR(Proc.read_srint(r[1]), Proc.read_Ri(r[2]));
            break;
          case XORSINT:
            Proc.get_srint_ref(r[0]).Bitwise_XOR(Proc.read_srint(r[1]), Proc.read_srint(r[2]));
            break;
          case XORSINTC:
            Proc.get_srint_ref(r[0]).Bitwise_XOR(Proc.read_srint(r[1]), Proc.read_Ri(r[2]));
            break;
          case INVSINT:
            Proc.get_srint_ref(r[0]).Bitwise_Negate(Proc.read_srint(r[1]));
            break;
          case XORSB:
            Proc.get_sbit_ref(r[0]).add(Proc.read_sbit(r[1]), Proc.read_sbit(r[2]));
            break;
          case ANDSB:
            Proc.temp.T= OTD.aAD.get_aAND(Proc.online_thread_num);
            Mult_aBit(Proc.get_sbit_ref(r[0]), Proc.read_sbit(r[1]), Proc.read_sbit(r[2]), Proc.temp.T, P);
            break;
          case ORSB:
            Proc.temp.T= OTD.aAD.get_aAND(Proc.online_thread_num);
            Mult_aBit(Proc.temp.aB, Proc.read_sbit(r[1]), Proc.read_sbit(r[2]), Proc.temp.T, P);
            Proc.get_sbit_ref(r[0]).add(Proc.read_sbit(r[1]), Proc.read_sbit(r[2]));
            Proc.get_sbit_ref(r[0]).add(Proc.temp.aB);
            break;
          case NEGB:
            Proc.get_sbit_ref(r[0]).negate(Proc.read_sbit(r[1]));
            break;
          case LTZSINT:
            Proc.get_sbit_ref(r[0])= Proc.read_srint(r[1]).less_than_zero();
            break;
          case EQZSINT:
            Proc.get_sbit_ref(r[0])= Proc.read_srint(r[1]).equal_zero(P, Proc.online_thread_num);
            break;
          case BITSINT:
            Proc.get_sbit_ref(r[0])= Proc.read_srint(r[1]).get_bit(n);
            break;
          case SINTBIT:
            Proc.get_srint_ref(r[0])= Proc.read_srint(r[1]);
            Proc.get_srint_ref(r[0]).set_bit(n, Proc.read_sbit(r[2]));
            break;
          case CONVSINTSREG:
            Proc.convert_sint_to_sregint(r[1], r[0], P);
            break;
          case CONVSREGSINT:
            Proc.convert_sregint_to_sint(r[1], r[0], P);
            break;
          case CONVREGSREG:
            Proc.write_srint(r[0], Proc.read_Ri(r[1]));
            break;
          case OPENSINT:
            Proc.write_Ri(r[0], Proc.read_srint(r[1]).Open(P));
            break;
          case OPENSBIT:
            int t;
            Open_aBit(t, Proc.read_sbit(r[1]), P);
            Proc.write_Ri(r[0], t);
            break;
          case GC:
            Proc.apply_GC(start, P);
            break;
          case LF_SINT:
            Proc.apply_local_function(MODP, SECRET, start);
            break;
          case LF_CINT:
            Proc.apply_local_function(MODP, CLEAR, start);
            break;
          case LF_SREGINT:
            Proc.apply_local_function(INT, SECRET, start);
            break;
          case LF_REGINT:
            Proc.apply_local_function(INT, CLEAR, start);
            break;
          default:
            printf("Invalid opcode=%d. Since it is not implemented\n", opcode);
            throw not_implemented();
            break;
        }
      if (size > 1)
        {
          r[0]++;
          r[1]++;
          r[2]++;
        }
    }
  return restart;
}
