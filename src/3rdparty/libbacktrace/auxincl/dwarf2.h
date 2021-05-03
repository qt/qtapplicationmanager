/* dwarf2.h -- minimal GCC dwarf2.h replacement for libbacktrace
   Contributed by Alexander Monakov, ISP RAS

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    (1) Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

    (2) Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.

    (3) The name of the author may not be used to
    endorse or promote products derived from this software without
    specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.  */

#ifndef BACKTRACE_AUX_DWARF2_H
#define BACKTRACE_AUX_DWARF2_H

/* Provide stub enum tags.  */
enum dwarf_attribute {_dummy_dwarf_attribute};
enum dwarf_form {_dummy_dwarf_form};
enum dwarf_tag {_dummy_dwarf_tag};

#define DW_AT_abstract_origin 0x31
#define DW_AT_call_file 0x58
#define DW_AT_call_line 0x59
#define DW_AT_comp_dir 0x1b
#define DW_AT_high_pc 0x12
#define DW_AT_linkage_name 0x6e
#define DW_AT_low_pc 0x11
#define DW_AT_MIPS_linkage_name 0x2007
#define DW_AT_name 0x03
#define DW_AT_namelist_item 0x44
#define DW_AT_ranges 0x55
#define DW_AT_ranges_base 0x74
#define DW_AT_specification 0x47
#define DW_AT_stmt_list 0x10
#define DW_FORM_addr 0x01
#define DW_FORM_addrx 0x1b
#define DW_FORM_block 0x09
#define DW_FORM_block1 0x0a
#define DW_FORM_block2 0x03
#define DW_FORM_block4 0x04
#define DW_FORM_data1 0x0b
#define DW_FORM_data16 0x1e
#define DW_FORM_data2 0x05
#define DW_FORM_data4 0x06
#define DW_FORM_data8 0x07
#define DW_FORM_exprloc 0x18
#define DW_FORM_flag 0x0c
#define DW_FORM_flag_present 0x19
#define DW_FORM_GNU_addr_index 0x1f01
#define DW_FORM_GNU_ref_alt 0x1f20
#define DW_FORM_GNU_str_index 0x1f02
#define DW_FORM_GNU_strp_alt 0x1f21
#define DW_FORM_indirect 0x16
#define DW_FORM_ref1 0x11
#define DW_FORM_ref2 0x12
#define DW_FORM_ref4 0x13
#define DW_FORM_ref8 0x14
#define DW_FORM_ref_addr 0x10
#define DW_FORM_ref_sig8 0x20
#define DW_FORM_ref_udata 0x15
#define DW_FORM_sdata 0x0d
#define DW_FORM_sec_offset 0x17
#define DW_FORM_string 0x08
#define DW_FORM_strp 0x0e
#define DW_FORM_strp_sup 0x1d
#define DW_FORM_udata 0x0f
#define DW_LNE_define_file 0x03
#define DW_LNE_define_file_MD5 0x05
#define DW_LNE_end_sequence 0x01
#define DW_LNE_set_address 0x02
#define DW_LNE_set_discriminator 0x04
#define DW_LNS_advance_line 0x03
#define DW_LNS_advance_pc 0x02
#define DW_LNS_const_add_pc 0x08
#define DW_LNS_copy 0x01
#define DW_LNS_extended_op 0x00
#define DW_LNS_fixed_advance_pc 0x09
#define DW_LNS_negate_stmt 0x06
#define DW_LNS_set_basic_block 0x07
#define DW_LNS_set_column 0x05
#define DW_LNS_set_epilogue_begin 0x0b
#define DW_LNS_set_file 0x04
#define DW_LNS_set_isa 0x0c
#define DW_LNS_set_prologue_end 0x0a
#define DW_TAG_compile_unit 0x11
#define DW_TAG_entry_point 0x03
#define DW_TAG_inlined_subroutine 0x1d
#define DW_TAG_subprogram 0x2e

#endif
