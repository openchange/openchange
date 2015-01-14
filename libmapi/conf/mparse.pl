#!/usr/bin/perl -w

###################################################
# package to parse the mapi-properties files and
# generate code for libmapi in OpenChange
#
# Perl code based on pidl one from Andrew Tridgell and the Samba team
#
# Copyright (C) Julien Kerihuel 2005-2011
# released under the GNU GPL

use strict;
use Getopt::Long;

my $ret = "";
my $tabs = "";

sub indent() { $tabs.="\t"; }
sub deindent() { $tabs = substr($tabs, 1); }
sub mparse($) { $ret .= $tabs.(shift)."\n"; }

my($opt_outputdir) = '.';
my($opt_parser) = '';

my	%prop_types = (
    0x0		=> "PT_UNSPECIFIED",
    0x1		=> "PT_NULL",
    0x2		=> "PT_SHORT",
    0x3		=> "PT_LONG",
    0x4		=> "PT_FLOAT",
    0x5		=> "PT_DOUBLE",
    0x6		=> "PT_CURRENCY",
    0x7		=> "PT_APPTIME",
    0xa		=> "PT_ERROR",
    0xb		=> "PT_BOOLEAN",
    0xd		=> "PT_OBJECT",
    0x14	=> "PT_I8",
    0x1e	=> "PT_STRING8",
    0x1f	=> "PT_UNICODE",
    0x40	=> "PT_SYSTIME",
    0x48	=> "PT_CLSID",
    0xFB	=> "PT_SVREID",
    0xFD	=> "PT_SRESTRICT",
    0xFE	=> "PT_ACTIONS",
    0x102	=> "PT_BINARY",
# Multi-valued property types
    0x1002	=> "PT_MV_SHORT",
    0x1003	=> "PT_MV_LONG",
    0x1004	=> "PT_MV_FLOAT",
    0x1005	=> "PT_MV_DOUBLE",
    0x1006	=> "PT_MV_CURRENCY",
    0x1007	=> "PT_MV_APPTIME",
    0x1014	=> "PT_MV_I8",
    0x101e	=> "PT_MV_STRING8",
    0x101f	=> "PT_MV_UNICODE",
    0x1040	=> "PT_MV_SYSTIME",
    0x1048	=> "PT_MV_CLSID",
    0x1102	=> "PT_MV_BINARY"
);

my	%prop_names = (
    "PT_UNSPECIFIED"	=>	0x0,
    "PT_NULL"		=>	0x1,
    "PT_SHORT"		=>	0x2,
    "PT_LONG"		=>	0x3,
    "PT_FLOAT"		=>	0x4,
    "PT_DOUBLE"		=>	0x5,
    "PT_CURRENCY"	=>	0x6,
    "PT_APPTIME"	=>	0x7,
    "PT_ERROR"		=>	0xa,
    "PT_BOOLEAN"	=>	0xb,
    "PT_OBJECT"		=>	0xd,
    "PT_I8"		=>	0x14,
    "PT_STRING8"	=>	0x1e,
    "PT_UNICODE"	=>	0x1f,
    "PT_SYSTIME"	=>	0x40,
    "PT_CLSID"		=>	0x48,
    "PT_SVREID"		=>	0xfb,
    "PT_SRESTRICT"	=>	0xfd,
    "PT_ACTIONS"	=>	0xfe,
    "PT_BINARY"		=>	0x102,
# Multi-valued property types
    "PT_MV_SHORT"	=>	0x1002,
    "PT_MV_LONG"	=>	0x1003,
    "PT_MV_FLOAT"	=>	0x1004,
    "PT_MV_DOUBLE"	=>	0x1005,
    "PT_MV_CURRENCY"	=>	0x1006,
    "PT_MV_APPTIME"	=>	0x1007,
    "PT_MV_I8"		=>	0x1014,
    "PT_MV_STRING8"	=>	0x101e,
    "PT_MV_UNICODE"	=>	0x101f,
    "PT_MV_SYSTIME"	=>	0x1040,
    "PT_MV_CLSID"	=>	0x1048,
    "PT_MV_BINARY"	=>	0x1102
);

my	%oleguid = (
    "PSETID_Appointment"	=>	"00062002-0000-0000-c000-000000000046",
    "PSETID_Task"		=>	"00062003-0000-0000-c000-000000000046",
    "PSETID_Address"		=>	"00062004-0000-0000-c000-000000000046",
    "PSETID_Common"		=>	"00062008-0000-0000-c000-000000000046",
    "PSETID_Note"		=>	"0006200e-0000-0000-c000-000000000046",
    "PSETID_Log"		=>	"0006200a-0000-0000-c000-000000000046",
    "PSETID_Sharing"		=>	"00062040-0000-0000-c000-000000000046",
    "PSETID_PostRss"		=>	"00062041-0000-0000-c000-000000000046",
    "PSETID_UnifiedMessaging"	=>	"4442858e-a9e3-4e80-b900-317a210cc15b",
    "PSETID_Meeting"		=>	"6ed8da90-450b-101b-98da-00aa003f1305",
    "PSETID_AirSync"		=>	"71035549-0739-4dcb-9163-00f0580dbbdf",
    "PSETID_Messaging"		=>	"41f28f13-83f4-4114-a584-eedb5a6b0bff",
    "PSETID_Attachment"		=>	"96357f7f-59e1-47d0-99a7-46515c183b54",
    "PSETID_CalendarAssistant"	=>	"11000e07-b51b-40d6-af21-caa85edab1d0",
    "PS_PUBLIC_STRINGS"		=>	"00020329-0000-0000-c000-000000000046",
    "PS_INTERNET_HEADERS"	=>	"00020386-0000-0000-c000-000000000046",
    "PS_MAPI"			=>	"00020328-0000-0000-c000-000000000046",
    "PSETID_Report"             =>      "00062013-0000-0000-c000-000000000046",
    "PSETID_Remote"             =>      "00062014-0000-0000-c000-000000000046",
    "PS_UNKNOWN_0006200b_0000_0000_c000_000000000046" => "0006200b-0000-0000-c000-000000000046",
    "PSETID_Appointment2"	=>	"02200600-0000-0000-c000-000000000046",
    "PSETID_GoodMessaging"	=>	"ef9203ee-b9a5-101b-acc1-00aa00423326",
    "PS_UNKNOWN_2f733904_5c72_45d7_a476_c4dd9f3f4052" => "2f733904-5c72-45d7-a476-c4dd9f3f4052"
);

# main program

my $result = GetOptions (
			 'outputdir=s' => \$opt_outputdir,
			 'parser=s' => \$opt_parser
			 );

if (not $result) {
    exit(1);
}

#####################################################################
# read a file into a string
sub FileLoad($)
{
    my($filename) = shift;
    local(*INPUTFILE);
    open(INPUTFILE, $filename) || return undef;
    my($saved_delim) = $/;
    undef $/;
    my($data) = <INPUTFILE>;
    close(INPUTFILE);
    $/ = $saved_delim;
    return $data;
}

#####################################################################
# write a string into a file
sub FileSave($$)
{
    my($filename) = shift;
    my($v) = shift;
    local(*FILE);
    open(FILE, ">$filename") || die "can't open $filename";
    print FILE $v;
    close(FILE);
}

#####################################################################
# generate mapitags.h file
sub mapitags_header($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @prop;
    my $prop_type;
    my $prop_value;

    mparse "/* parser auto-generated by mparse */";
    mparse "#ifndef __MAPITAGS_H__";
    mparse "#define __MAPITAGS_H__";
    mparse "";

    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @prop = split(/\s+/, $line);
	    # if (!defined($prop[6])) {
	    #     print "prop6 not defined for line: $line\n";
	    # }
	    # elsif (!defined($oleguid{$prop[6]})) {
	    #     print "entry not defined for guid: $prop[6]\n";
	    # }
	    $prop_type = hex $prop[0];
	    $prop_type &= 0xFFFF;
	    $prop_value = hex $prop[0];
	    $prop_value = ($prop_value >> 16) & 0xFFFF;

	    mparse sprintf "#define %-51s PROP_TAG(%-13s, 0x%.04x) /* %s */", $prop[1], $prop_types{$prop_type}, $prop_value, $prop[0] if ($prop_types{$prop_type});
	    if (($prop_type == 0x1e) || ($prop_type == 0x101e)){
		$prop_type++;
		$prop[0] = sprintf "0x%.8x", ((hex $prop[0]) & 0xFFFF0000) + $prop_type;
		mparse sprintf "#define %-51s PROP_TAG(%-13s, 0x%.04x) /* %s */", "$prop[1]_UNICODE", $prop_types{$prop_type}, $prop_value, $prop[0] if ($prop_types{$prop_type});
	    }
	    $prop_type = 0xa;
	    $prop[0] = sprintf "0x%.8x", ((hex $prop[0]) & 0xFFFF0000) + $prop_type;
	    mparse sprintf "#define %-51s PROP_TAG(%-13s, 0x%.04x) /* %s */", "$prop[1]_ERROR", $prop_types{$prop_type}, $prop_value, $prop[0] if ($prop_types{$prop_type});
	}
    }
    mparse "";
    mparse "#endif /* !__MAPITAGS_H__ */";

    return $ret;

}


#####################################################################
# generate mapitags.c file
sub mapitags_interface($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @prop;
    my $prop_type;
    my $prop_value;

    mparse "/* parser auto-generated by mparse */";
    mparse "#include \"libmapi/libmapi.h\"";
    mparse "#include \"libmapi/libmapi_private.h\"";
    mparse "#include \"gen_ndr/ndr_exchange.h\"";
    mparse "#include \"libmapi/mapitags.h\"";
    mparse "";
    mparse "struct mapi_proptags";
    mparse "{";
    indent;
    mparse "uint32_t	proptag;";
    mparse "uint32_t	proptype;";
    mparse "const char	*propname;";
    deindent;
    mparse "};";
    mparse "";
    mparse "static struct mapi_proptags mapitags[] = {";
    indent;

    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @prop = split(/\s+/, $line);
	    $prop_type = hex $prop[0];
	    $prop_type &= 0xFFFF;
	    $prop_value = hex $prop[0];
	    $prop_value = ($prop_value >> 16) & 0xFFFF;
	    if ($prop_types{$prop_type}) {
		mparse sprintf "{ %-51s, %-13s, \"%s\" },", $prop[1], $prop_types{$prop_type}, $prop[1];
		if (($prop_type == 0x1e) || ($prop_type == 0x101e)) {
		    $prop_type++;
		    mparse sprintf "{ %-51s, %-13s, \"%s\" },", "$prop[1]_UNICODE", $prop_types{$prop_type}, "$prop[1]_UNICODE";
		}
		$prop_type = 0xa;
		mparse sprintf "{ %-51s, %-13s, \"%s\" },", "$prop[1]_ERROR", $prop_types{$prop_type}, $prop[1];
	    }
	}
    }

    mparse sprintf "{ %-51s, %-13d, \"NULL\"}", 0, 0;
    deindent;
    mparse "};";
    mparse "";
    mparse "_PUBLIC_ const char *get_proptag_name(uint32_t proptag)";
    mparse "{";
    indent;
    mparse "uint32_t idx;";
    mparse "";
    mparse "for (idx = 0; mapitags[idx].proptag; idx++) {";
    indent;
    mparse "if (mapitags[idx].proptag == proptag) { ";
    indent;
    mparse "return mapitags[idx].propname;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return NULL;";
    deindent;
    mparse "}";
    mparse "";
    mparse "_PUBLIC_ uint32_t get_proptag_value(const char *propname)";
    mparse "{";
    indent;
    mparse "uint32_t idx;";
    mparse "";
    mparse "for (idx = 0; mapitags[idx].proptag; idx++) {";
    indent;
    mparse "if (!strcmp(mapitags[idx].propname, propname)) { ";
    indent;
    mparse "return mapitags[idx].proptag;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return 0;";
    deindent;
    mparse "}";
    mparse "";

    mparse "_PUBLIC_ uint16_t get_property_type(uint16_t untypedtag)";
    mparse "{";
    indent;
    mparse "uint32_t idx;";
    mparse "uint16_t current_type;";
    mparse "";
    mparse "for (idx = 0; mapitags[idx].proptag; idx++) {";
    indent;
    mparse "if ((mapitags[idx].proptag >> 16) == untypedtag) {";
    indent;
    mparse "current_type = mapitags[idx].proptag & 0xffff;";
    mparse "if (current_type != PT_ERROR && current_type != PT_STRING8) {";
    indent;
    mparse "return current_type;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "DEBUG(5, (\"%s: type for property '%x' could not be deduced\\n\", __FUNCTION__, untypedtag));";
    mparse "";
    mparse "return 0;";
    deindent;
    mparse "}";

    return $ret;
}

#####################################################################
# generate mapitags_enum.idl file
sub mapitags_enum($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @prop;
    my $prop_type;
    my %hash;

    mparse "/* parser auto-generated by mparse */";
    mparse "";

    mparse "typedef [v1_enum, flag(NDR_PAHEX)] enum {";
    indent;

    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @prop = split(/\s+/, $line);
	    $prop_type = hex $prop[0];
	    $prop_type &= 0xFFFF;

	    mparse sprintf "%-51s = %s,", $prop[1], $prop[0];

	    if (($prop_type == 0x1e) || ($prop_type == 0x101e)) {
		$prop_type = hex $prop[0];
		$prop_type++;
		mparse sprintf "%-51s = 0x%.8x,", "$prop[1]_UNICODE", $prop_type;
	    }
	    if (!exists($hash{((hex $prop[0]) & 0xFFFF0000)})) {
		$prop_type = 0xa;
		$prop_type += ((hex $prop[0]) & 0xFFFF0000);
		mparse sprintf "%-51s = 0x%.8x,", "$prop[1]_ERROR", $prop_type;
		$hash{((hex $prop[0]) & 0xFFFF0000)} = 1;
	    }
	}
    }
    mparse sprintf "%-51s = %s", "MAPI_PROP_RESERVED", "0xFFFFFFFF";
    deindent;
    mparse "} MAPITAGS;";
    mparse "";

    return $ret;
}


#####################################################################
# generate mapi_nameid_private.h file
sub mapi_nameid_private_header($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @prop;
    my $prop_OOM;
    my $prop_ID;
    my $prop_Type;
    my $prop_name;
    my $prop_Kind;
    my $prop_OLEGUID;

    mparse "/* parser auto-generated by mparse */";
    mparse "#ifndef __MAPI_NAMEID_PRIVATE_H__";
    mparse "#define __MAPI_NAMEID_PRIVATE_H__";
    mparse "";
    mparse "static struct mapi_nameid_tags mapi_nameid_tags[] = {";
    indent;

    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @prop = split(/\s+/, $line);

	    if ($prop[1] ne "NULL") {
		$prop[1] = sprintf "\"%s\"", $prop[1];
	    }
	    if ($prop[3] ne "NULL") {
		$prop[3] = sprintf "\"%s\"", $prop[3]
	    }
	    if ($prop[0] eq "NULL") {
		$prop[0] = "0x00000000";
	    }
	    mparse sprintf "{ %-51s, %-40s, %-15s, %-30s, %-30s, %-15s, %-20s, 0x0 },",
	    $prop[0], $prop[1], $prop[2], $prop[3], $prop[4], $prop[5], $prop[6];
	}
    }

    mparse sprintf "{ 0x00000000, NULL, 0x0, NULL, PT_UNSPECIFIED, 0x0, NULL, 0x0 }";
    deindent;
    mparse "};";
    mparse "";
    mparse "#endif /* !MAPI_NAMEID_PRIVATE_H__ */";

    return $ret;
}

#####################################################################
# generate mapi_nameid.h file
sub mapi_nameid_header($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @prop;
    my $propID;
    my $proptype;
    my $property;
    my $counter = 0;

    mparse "/* parser auto-generated by mparse */";
    mparse "#ifndef __MAPI_NAMEID_H__";
    mparse "#define __MAPI_NAMEID_H__";
    mparse "";

    mparse "/* NOTE TO DEVELOPERS: If this is a MNID_STRING named property,";
    mparse " * then we use the arbitrary 0xa000-0afff property ID range for";
    mparse " * internal mapping purpose only.";
    mparse " */";

    mparse "";
    mparse "struct mapi_nameid_tags {";
    indent;
    mparse "uint32_t			proptag;";
    mparse "const char			*OOM;";
    mparse "uint16_t			lid;";
    mparse "const char			*Name;";
    mparse "uint32_t			propType;";
    mparse "uint8_t				ulKind;";
    mparse "const char			*OLEGUID;";
    mparse "uint32_t			position;";
    deindent;
    mparse "};";
    mparse "";
    mparse "struct mapi_nameid {";
    indent;
    mparse "struct MAPINAMEID		*nameid;";
    mparse "uint16_t			count;";
    mparse "struct mapi_nameid_tags		*entries;";
    deindent;
    mparse "};";
    mparse "";

    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @prop = split(/\s+/, $line);
	    if ($prop[0] ne "NULL") {
		$proptype = $prop_names{$prop[4]};
		$propID = hex $prop[2];
		if ($propID == 0) {
		    # 40960 == 0xa000
		    $propID = 40960 + $counter;
		    $counter++;
		}
		$property = sprintf "0x%.8x", ($propID << 16) | $proptype;
		mparse sprintf "#define %-51s %s", $prop[0], $property;
	    }
	}
    }

    mparse "";
    mparse "#endif /* !MAPI_NAMEID_H__ */";

    return $ret;
}

#####################################################################
# generate mapicode.c file

sub mapicodes_interface($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @errors;

    mparse "/* parser auto-generated by mparse */";
    mparse "#include \"libmapi/libmapi.h\"";
    mparse "#include \"libmapi/libmapi_private.h\"";
    mparse "#include \"gen_ndr/ndr_exchange.h\"";
    mparse "";
    mparse "void set_errno(enum MAPISTATUS status)";
    mparse "{";
    indent;
    mparse "errno = status;";
    deindent;
    mparse "}";
    mparse "";
    mparse "struct mapi_retval {";
    indent;
    mparse "enum MAPISTATUS		err;";
    mparse "const char		*name;";
    deindent;
    mparse "};";
    mparse "";
    mparse "static const struct mapi_retval mapi_retval[] = {";
    indent;

    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @errors = split(/\s+/, $line);
	    mparse sprintf "{ %8s, \"%s\" },", $errors[1], $errors[1];
	}
    }


    mparse " { MAPI_E_RESERVED, NULL }";
    deindent;
    mparse "};";
    mparse "";
    mparse "_PUBLIC_ void mapi_errstr(const char *function, enum MAPISTATUS mapi_code)";
    mparse "{";
    indent;
    mparse "struct ndr_print	ndr_print;";
    mparse "";
    mparse "ndr_print.depth = 1;";
    mparse "ndr_print.print = ndr_print_debug_helper;";
    mparse "ndr_print.no_newline = false;";
    mparse "ndr_print_MAPISTATUS(&ndr_print, function, mapi_code);";
    deindent;
    mparse "}";
    mparse "";
    mparse "_PUBLIC_ const char *mapi_get_errstr(enum MAPISTATUS mapi_code)";
    mparse "{";
    indent;
    mparse "uint32_t i;";
    mparse "";
    mparse "for (i = 0; mapi_retval[i].name; i++) {";
    indent;
    mparse "if (mapi_retval[i].err == mapi_code) {";
    indent;
    mparse "return mapi_retval[i].name;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return NULL;";
    deindent;
    mparse "}";
}

#####################################################################
# generate mapicodes_enum.idl file
sub mapicodes_enum($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @prop;
    my $prop_type;

    mparse "/* parser auto-generated by mparse */";
    mparse "";

    mparse "typedef [public, v1_enum, flag(NDR_PAHEX)] enum {";
    indent;

    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @prop = split(/\s+/, $line);
	    mparse sprintf "%-51s = %s,", $prop[1], $prop[0];
	}
    }
    mparse sprintf "%-51s = %s", "MAPI_E_RESERVED", "0xFFFFFFFF";
    deindent;
    mparse "} MAPISTATUS;";
    mparse "";

    return $ret;
}

#####################################################################
# generate openchangedb_property.c file
sub openchangedb_property($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @prop;
    my $prop_type;
    my $prop_value;
    my $pidtag;

    mparse "/* parser auto-generated by mparse */";
    mparse "#include \"mapiproxy/dcesrv_mapiproxy.h\"";
    mparse "#include \"libmapiproxy.h\"";
    mparse "#include \"libmapi/libmapi.h\"";
    mparse "#include \"libmapi/libmapi_private.h\"";
    mparse "";
    mparse "struct pidtags {";
    mparse "	uint32_t	proptag;";
    mparse "	const char	*pidtag;";
    mparse "};";
    mparse "";
    mparse "static struct pidtags pidtags[] = {";
    indent;

    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @prop = split(/\s+/, $line);
	    $prop_type = hex $prop[0];
	    $prop_type &= 0xFFFF;
	    $prop_value = hex $prop[0];
	    $prop_value = ($prop_value >> 16) & 0xFFFF;
	    if ($prop_types{$prop_type}) {
		if ($prop[2]) {
		    mparse sprintf "{ %-51s, \"%s\"},", $prop[1], $prop[2];
		} else {
		    mparse sprintf "{ %-51s, \"0x%.8x\"},", $prop[1], hex $prop[0];
		}

		if (($prop_type == 0x1e) ||  ($prop_type == 0x101e)) {
		    if ($prop[2]) {
			mparse sprintf "{ %-51s, \"%s\"},", "$prop[1]_UNICODE", $prop[2];
		    } else {
			mparse sprintf "{ %-51s, \"0x%.8x\"},", "$prop[1]_UNICODE", hex $prop[0];
		    }
		}
	    }
	}
    }

    mparse sprintf "{ %-51s, NULL }", 0;
    deindent;
    mparse "};";
    mparse "";
    mparse "_PUBLIC_ const char *openchangedb_property_get_attribute(uint32_t proptag)";
    mparse "{";
    indent;
    mparse "uint32_t i;";
    mparse "";
    mparse "for (i = 0; pidtags[i].pidtag; i++) {";
    indent;
    mparse "if (pidtags[i].proptag == proptag) {";
    indent;
    mparse "return pidtags[i].pidtag;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "DEBUG(0, (\"[%s:%d]: Unsupported property tag '0x%.8x'\\n\", __FUNCTION__, __LINE__, proptag));";
    mparse "";
    mparse "return NULL;";
    deindent;
    mparse "}";
}

#####################################################################
# generate codepage_lcid.c file
sub codepage_lcid_interface($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @params;
    my $locale;

    mparse "/* parser auto-generated by mparse */";
    mparse "#include \"libmapi/libmapi.h\"";
    mparse "#include \"libmapi/libmapi_private.h\"";
    mparse "#include <locale.h>";
    mparse "#include <langinfo.h>";
    mparse "";
    mparse "/**";
    mparse "  \\file codepage_lcid.c";
    mparse "";
    mparse "  \\brief Codepage and Locale ID operations";
    mparse " */";
    mparse "";

    # Step 1. Generate code for defines
    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @params = split(/\s+/, $line);
	    if ($params[0] eq "DEFINE") {
		mparse sprintf "#define	%-30s %15d", $params[1], $params[2];
	    }
	}
    }

## We do not have yet convenient functions making use of this struct. This causes warning
#    mparse "";
#    mparse "static const char *language_group[] =";
#    mparse "{";
#    indent;
#    foreach $line (@lines) {
#	$line =~ s/^\#+.*$//;
#	if ($line) {
#	    @params = split(/\s+/, $line);
#	    if ($params[0] eq "DEFINE") {
#		mparse sprintf "\"%s\",", $params[1];
#	    }
#	}
#    }
#    mparse "NULL";
#    deindent;
#    mparse "};";
#    mparse "";

    # Step 2. Generate the locales array
    mparse "struct cpid_lcid {";
    indent;
    mparse "const char	*language;";
    mparse "const char	*locale;";
    mparse "uint32_t	lcid;";
    mparse "uint32_t	cpid;";
    mparse "uint32_t	language_group;";
    deindent;
    mparse "};";
    mparse "";

    mparse "static const struct cpid_lcid locales[] =";
    mparse "{";
    indent;

    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @params = split(/\s+/, $line);
	    if ($params[0] ne "DEFINE") {

		$params[0] = ($params[1] eq "NULL") ? (sprintf "\"%s\",", $params[0]) :
		    (sprintf "\"%s (%s)\",", $params[0], $params[1]);
		$params[0] =~ s/_/ /g;
		$params[2] = sprintf "\"%s\",", $params[2];
		mparse sprintf "{ %-32s %-18s %-6s, %-4s, %-24s },",
		$params[0], $params[2], $params[3], $params[4], $params[5];
	    }
	}
    }

    mparse q|{ "C",                             "C",               0x0409, 1252, CP_WESTERN_EUROPE_AND_US },|;
    mparse "{ NULL, NULL, 0, 0, 0 }";
    deindent;
    mparse "};";
    mparse "";

    # mapi_get_system_locale
    mparse "/**";
    mparse "  \\details Returns current locale used by the system";
    mparse "";
    mparse " \\return pointer to locale string on success, otherwise NULL";
    mparse " */";
    mparse "_PUBLIC_ char *mapi_get_system_locale(void)";
    mparse "{";
    indent;
    mparse "char	*locale;";
    mparse "";
    mparse "locale = setlocale(LC_CTYPE, \"\");";
    mparse "return locale;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_verify_cpid
    mparse "/**";
    mparse "  \\details Verify if the specified codepage is valid";
    mparse "";
    mparse "  \\param cpid the codepage to lookup";
    mparse "";
    mparse "  \\return 0 on success, otherwise 1";
    mparse " */";
    mparse "_PUBLIC_ bool mapi_verify_cpid(uint32_t cpid)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "for (i = 0; locales[i].lcid; i++) {";
    indent;
    mparse "if (cpid == locales[i].cpid) {";
    indent;
    mparse "return true;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return false;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_get_cpid_from_lcid
    mparse "/**";
    mparse "  \\details Returns codepage for a given LCID (Locale ID)";
    mparse "";
    mparse "  \\param lcid the locale ID to lookup";
    mparse "";
    mparse "  \\return non-zero codepage on success, otherwise 0 if";
    mparse "   only unicode is supported for this language";
    mparse " */";
    mparse "_PUBLIC_ uint32_t mapi_get_cpid_from_lcid(uint32_t lcid)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "for (i = 0; locales[i].lcid; i++) {";
    indent;
    mparse "if (lcid == locales[i].lcid) {";
    indent;
    mparse "return locales[i].cpid;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return 0;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_get_cpid_from_locale
    mparse "/**";
    mparse "  \\details Return codepage associated to specified locale";
    mparse "";
    mparse "  \\param locale The locale string to lookup";
    mparse "";
    mparse "  \\return non-zero codepage on success, otherwise 0";
    mparse " */";
    mparse "_PUBLIC_ uint32_t mapi_get_cpid_from_locale(const char *locale)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "/* Sanity Checks */";
    mparse "if (!locale) return 0;";
    mparse "";
    mparse "for (i = 0; locales[i].locale; i++) {";
    indent;
    mparse "if (locales[i].locale && !strncmp(locales[i].locale, locale, strlen(locales[i].locale))) {";
    indent;
    mparse "return locales[i].cpid;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return 0;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_get_cpid_from_language
    mparse "/**";
    mparse "  \\details Return codepage associated to specified language";
    mparse "";
    mparse "  \\param language The language string to lookup";
    mparse "";
    mparse "  \\return non-zero codepage on success, otherwise 0";
    mparse " */";
    mparse "_PUBLIC_ uint32_t mapi_get_cpid_from_language(const char *language)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "/* Sanity Checks */";
    mparse "if (!language) return 0;";
    mparse "";
    mparse "for (i = 0; locales[i].language; i++) {";
    indent;
    mparse "if (locales[i].language && !strncmp(locales[i].language, language, strlen(locales[i].language))) {";
    indent;
    mparse "return locales[i].cpid;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return 0;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_get_lcid_from_locale
    mparse "/**";
    mparse "  \\details Returns LCID (Locale ID) for a given locale";
    mparse "";
    mparse "  \\param locale the locale string to lookup";
    mparse "";
    mparse "  \\return non-zero LCID on success, otherwise 0";
    mparse " */";
    mparse "_PUBLIC_ uint32_t mapi_get_lcid_from_locale(const char *locale)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "/* Sanity Checks */";
    mparse "if (!locale) return 0;";
    mparse "";
    mparse "for (i = 0; locales[i].locale; i++) {";
    indent;
    mparse "if (locales[i].locale && !strncmp(locales[i].locale, locale, strlen(locales[i].locale))) {";
    indent;
    mparse "return locales[i].lcid;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return 0;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_get_lcid_from_language
    mparse "/**";
    mparse "  \\details Returns LCID (Locale ID) for a given language";
    mparse "";
    mparse "  \\param language the language string to lookup";
    mparse "";
    mparse "  \\return non-zero LCID on success, otherwise 0";
    mparse " */";
    mparse "_PUBLIC_ uint32_t mapi_get_lcid_from_language(const char *language)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "/* Sanity Checks */";
    mparse "if (!language) return 0;";
    mparse "";
    mparse "for (i = 0; locales[i].language; i++) {";
    indent;
    mparse "if (locales[i].language && !strncmp(locales[i].language, language, strlen(locales[i].language))) {";
    indent;
    mparse "return locales[i].lcid;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return 0;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_get_locale_from_lcid
    mparse "/**";
    mparse "  \\details Returns Locale for a given Locale ID";
    mparse "";
    mparse "  \\param lcid the locale ID to lookup";
    mparse "";
    mparse "  \\return locale string on success, otherwise NULL";
    mparse " */";
    mparse "_PUBLIC_ const char *mapi_get_locale_from_lcid(uint32_t lcid)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "for (i = 0; locales[i].lcid; i++) {";
    indent;
    mparse "if (locales[i].lcid == lcid) {";
    indent;
    mparse "return locales[i].locale;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return NULL;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_get_locale_from_language
    mparse "/**";
    mparse "  \\details Returns Locale for a given language";
    mparse "";
    mparse "  \\param language the language string to lookup";
    mparse "";
    mparse "  \\return Locale string on success, otherwise NULL";
    mparse " */";
    mparse "_PUBLIC_ const char *mapi_get_locale_from_language(const char *language)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "/* Sanity Checks */";
    mparse "if (!language) return NULL;";
    mparse "";
    mparse "for (i = 0; locales[i].language; i++) {";
    indent;
    mparse "if (locales[i].language && !strncmp(locales[i].language, language, strlen(locales[i].language))) {";
    indent;
    mparse "return locales[i].locale;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return NULL;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_get_language_from_locale
       mparse "/**";
    mparse "  \\details Returns Language for a given Locale";
    mparse "";
    mparse "  \\param locale the language string to lookup";
    mparse "";
    mparse "  \\return Language string on success, otherwise NULL";
    mparse " */";
    mparse "_PUBLIC_ const char *mapi_get_language_from_locale(const char *locale)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "/* Sanity Checks */";
    mparse "if (!locale) return NULL;";
    mparse "";
    mparse "for (i = 0; locales[i].locale; i++) {";
    indent;
    mparse "if (locales[i].locale && !strncmp(locales[i].locale, locale, strlen(locales[i].locale))) {";
    indent;
    mparse "return locales[i].language;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return NULL;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_get_language_from_lcid
    mparse "/**";
    mparse "  \\details Returns Language for a given Locale ID";
    mparse "";
    mparse "  \\param lcid the locale ID to lookup";
    mparse "";
    mparse "  \\return language string on success, otherwise NULL";
    mparse " */";
    mparse "_PUBLIC_ const char *mapi_get_language_from_lcid(uint32_t lcid)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "for (i = 0; locales[i].lcid; i++) {";
    indent;
    mparse "if (locales[i].lcid == lcid) {";
    indent;
    mparse "return locales[i].language;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "return NULL;";
    deindent;
    mparse "}";
    mparse "";

    # mapi_get_language_by_group
        mparse "/**";
    mparse "  \\details Returns List of languages for a given Language Group";
    mparse "";
    mparse "  \\param mem_ctx pointer to the memory context";
    mparse "  \\param group the locale group to lookup";
    mparse "";
    mparse "  \\return Array of languages string on success, otherwise NULL";
    mparse " */";
    mparse "_PUBLIC_ char **mapi_get_language_from_group(TALLOC_CTX *mem_ctx, uint32_t group)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "uint32_t	counter = 0;";
    mparse "char		**languages;";
    mparse "";
    mparse "/* Sanity Checks */";
    mparse "if (!mem_ctx) return NULL;";
    mparse "";
    mparse "languages = talloc_array(mem_ctx, char *, counter + 1);";
    mparse "for (i = 0; locales[i].language; i++) {";
    indent;
    mparse "if (locales[i].language_group == group) {";
    indent;
    mparse "languages = talloc_realloc(mem_ctx, languages, char *, counter + 1);";
    mparse "languages[counter] = talloc_strdup(languages, locales[i].language);";
    mparse "counter += 1;";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
    mparse "";
    mparse "if (!counter) {";
    indent;
    mparse "talloc_free(languages);";
    mparse "return NULL;";
    deindent;
    mparse "}";
    mparse "";
    mparse "return languages;";
    deindent;
    mparse "}";
    mparse "";

    # assessor for mapidump_get_languages
    mparse "void mapi_get_language_list(void)";
    mparse "{";
    indent;
    mparse "uint32_t	i;";
    mparse "";
    mparse "for (i = 0; locales[i].language; i++) {";
    indent;
    mparse "printf(\"%s\\n\", locales[i].language);";
    deindent;
    mparse "}";
    deindent;
    mparse "}";
}



#####################################################################
# generate mapistore_namedprops.ldif file
sub mapistore_namedprops($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @prop;

    mparse "# LDIF file auto-generated by mparse ";
    mparse "";

    mparse sprintf "dn: CN=default";
    mparse sprintf "objectClass: top";
    mparse sprintf "cn: default";
    mparse sprintf "";

    mparse sprintf "dn: CN=custom";
    mparse sprintf "objectClass: top";
    mparse sprintf "cn: custom";
    mparse sprintf "";

    for my $key (sort(keys %oleguid)) {
        my $value = $oleguid{$key};

        mparse sprintf "dn: CN=%s,CN=default", $value;
        mparse sprintf "cn: %s", $value;
        mparse sprintf "name: %s", $key;
        mparse sprintf "oleguid: %s", $value;
        mparse "";
    }

    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
        $line =~ s/^\#+.*$//;
        if ($line) {
            @prop = split(/\s+/, $line);
            if ($prop[5] eq "MNID_ID" && $prop[7]) {
                mparse sprintf "dn: CN=%s,CN=%s,CN=default", $prop[2], $oleguid{$prop[6]};
                mparse sprintf "objectClass: %s", $prop[5];
                mparse sprintf "cn: %s", $prop[2];
                mparse sprintf "canonical: %s", $prop[0];
                mparse sprintf "oleguid: %s", $oleguid{$prop[6]};
                mparse sprintf "mappedId: %d", hex($prop[7]);
                mparse sprintf "propId: %d", hex($prop[2]);
                mparse sprintf "propType: %s", $prop_names{$prop[4]};
                mparse sprintf "oom: %s", $prop[1];
                mparse sprintf "";
            } elsif ($prop[5] eq "MNID_STRING" && $prop[7]) {
                mparse sprintf "dn: CN=%s,CN=%s,CN=default", $prop[3], $oleguid{$prop[6]};
                mparse sprintf "objectClass: %s", $prop[5];
                mparse sprintf "cn: %s", $prop[3];
                mparse sprintf "canonical: %s", $prop[0];
                mparse sprintf "oleguid: %s", $oleguid{$prop[6]};
                mparse sprintf "mappedId: %d", hex($prop[7]);
                mparse sprintf "propId: 0";
                mparse sprintf "propType: %s", $prop[4];
                mparse sprintf "propName: %s", $prop[3];
                mparse sprintf "";
            }
        }
    }
    return $ret;
}

#####################################################################
# generate mapistore_nameid.h file
sub mapistore_nameid_header($)
{
    my $contents = shift;
    my $line;
    my @lines;
    my @prop;
    my $propID;
    my $proptype;
    my $property;
    my $counter = 0;

    mparse "/* header auto-generated by mparse */";
    mparse "#ifndef __MAPISTORE_NAMEID_H__";
    mparse "#define __MAPISTORE_NAMEID_H__";
    mparse "";

    @lines = split(/\n/, $contents);
    foreach $line (@lines) {
	$line =~ s/^\#+.*$//;
	if ($line) {
	    @prop = split(/\s+/, $line);
	    if ($prop[0] ne "NULL" && $prop[0] !~ m/^PidLidUnknownProperty/
		&& defined($prop[7])) {
		$proptype = $prop_names{$prop[4]};
		my $propID = hex $prop[7];
		$property = sprintf "0x%.8x", ($propID << 16) | $proptype;
		mparse sprintf "#define %-51s %s", $prop[0], $property;
	    }
	}
    }

    mparse "";
    mparse "#endif /* !MAPISTORE_NAMEID_H__ */";

    return $ret;
}

sub process_file($)
{
    my $mapi_file = shift;
    my $outputdir = $opt_outputdir;

    print "Parsing $mapi_file\n";
    my $contents = FileLoad($mapi_file);
    defined $contents || return undef;


    if ($opt_parser eq "mapitags") {
	print "Generating $outputdir" . "mapitags.h\n";
	my $parser = ("$outputdir/mapitags.h");
	FileSave($parser, mapitags_header($contents));

	print "Generating $outputdir" . "mapitags.c\n";
	$ret = '';
	my $code_parser = ("$outputdir/mapitags.c");
	FileSave($code_parser, mapitags_interface($contents));

	print "Generating mapitags_enum.h\n";
	$ret = '';
	my $enum_parser = ("mapitags_enum.h");
	FileSave($enum_parser, mapitags_enum($contents));
    }

    if ($opt_parser eq "mapicodes") {
	print "Generating $outputdir" . "mapicode.c\n";
	$ret = '';
	my $code_parser = ("$outputdir/mapicode.c");
	FileSave($code_parser, mapicodes_interface($contents));

	print "Generating mapicodes_enum.h\n";
	$ret = '';
	my $enum_parser = ("mapicodes_enum.h");
	FileSave($enum_parser, mapicodes_enum($contents));
    }

    if ($opt_parser eq "mapi_nameid") {
	print "Generating $outputdir" . "mapi_nameid_private.h\n";
	$ret = '';
	my $parser = ("$outputdir/mapi_nameid_private.h");
	FileSave($parser, mapi_nameid_private_header($contents));

	print "Generating $outputdir" . "mapi_nameid.h\n";
	$ret = '';
	my $nameid_parser = ("$outputdir/mapi_nameid.h");
	FileSave($nameid_parser, mapi_nameid_header($contents));
    }

    if ($opt_parser eq "codepage_lcid") {
	print "Generating $outputdir" . "codepage_lcid.c\n";
	$ret = '';
	my $code_parser = ("$outputdir/codepage_lcid.c");
	FileSave($code_parser, codepage_lcid_interface($contents));
    }

    if ($opt_parser eq "openchangedb_property") {
	print "Generating $outputdir" . "openchangedb_property.c\n";
	my $openchangedb_parser = ("$outputdir/openchangedb_property.c");
	FileSave($openchangedb_parser, openchangedb_property($contents));
    }

    if ($opt_parser eq "mapistore_namedprops") {
	print "Generating $outputdir" . "mapistore_namedprops.ldif\n";
	my $mapistore_parser = ("$outputdir/mapistore_namedprops.ldif");
	FileSave($mapistore_parser, mapistore_namedprops($contents));
    }

    if ($opt_parser eq "mapistore_nameid") {
	print "Generating $outputdir" . "mapistore_nameid.h\n";
	my $mapistore_parser = ("$outputdir/mapistore_nameid.h");
	FileSave($mapistore_parser, mapistore_nameid_header($contents));
    }
}

process_file($_) foreach (@ARGV);
