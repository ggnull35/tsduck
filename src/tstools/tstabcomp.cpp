//----------------------------------------------------------------------------
//
// TSDuck - The MPEG Transport Stream Toolkit
// Copyright (c) 2005-2017, Thierry Lelegard
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice,
//    this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.
//
//----------------------------------------------------------------------------
//
//  PSI/SI tables compiler. 
//
//----------------------------------------------------------------------------

#include "tsArgs.h"
#include "tsSysUtils.h"
#include "tsStringUtils.h"
#include "tsBinaryTable.h"
#include "tsXMLTables.h"
#include "tsDVBCharset.h"
#include "tsReportWithPrefix.h"
#include "tsInputRedirector.h"
#include "tsOutputRedirector.h"
TSDUCK_SOURCE;


//----------------------------------------------------------------------------
//  Command line options
//----------------------------------------------------------------------------

struct Options: public ts::Args
{
    Options(int argc, char *argv[]);

    ts::StringVector      infiles;         // Input file names.
    std::string           outfile;         // Output file path.
    bool                  outdir;          // Output name is a directory.
    bool                  compile;         // Explicit compilation.
    bool                  decompile;       // Explicit decompilation.
    bool                  xmlModel;        // Display XML model instead of compilation.
    const ts::DVBCharset* defaultCharset;  // Default DVB character set to interpret strings.

private:
    // Inaccessible operations.
    Options(const Options&) = delete;
    Options& operator=(const Options&) = delete;
};

Options::Options(int argc, char *argv[]) :
    ts::Args("PSI/SI tables compiler.", "[options] filename ..."),
    infiles(),
    outfile(),
    outdir(false),
    compile(false),
    decompile(false),
    xmlModel(false),
    defaultCharset(0)
{
    option("",                0,  ts::Args::STRING);
    option("compile",        'c');
    option("decompile",      'd');
    option("default-charset", 0, Args::STRING);
    option("output",         'o', ts::Args::STRING);
    option("verbose",        'v');
    option("xml-model",      'x');

    setHelp("Input files:\n"
            "\n"
            "  XML source files to compile or binary table files to decompile. By default,\n"
            "  files ending in .xml are compiled and files ending in .bin are decompiled.\n"
            "  For other files, explicitly specify --compile or --decompile.\n"
            "\n"
            "Options:\n"
            "\n"
            "  -c\n"
            "  --compile\n"
            "      Compile all files as XML source files into binary files. This is the\n"
            "      default for .xml files.\n"
            "\n"
            "  -d\n"
            "  --decompile\n"
            "      Decompile all files as binary files into XML files. This is the default\n"
            "      for .bin files.\n"
            "\n"
            "  --default-charset name\n"
            "      Default DVB character set to use. The available table names are:\n"
            "      " + ts::UString::Join(ts::DVBCharset::GetAllNames()).toSplitLines(74, ts::UString(), ts::UString(6, ts::SPACE)).toUTF8() + ".\n"
            "\n"
            "      With --compile, this character set is used to encode strings. If a\n"
            "      given string cannot be encoded with this character set or if this option\n"
            "      is not specified, an appropriate character set is automatically selected.\n"
            "\n"
            "      With --decompile, this character set is used to interpret DVB strings\n"
            "      without explicit character table code. According to DVB standard ETSI EN\n"
            "      300 468, the default DVB character set is ISO-6937. However, some bogus\n"
            "      signalization may assume that the default character set is different,\n"
            "      typically the usual local character table for the region. This option\n"
            "      forces a non-standard character table.\n"
            "\n"
            "  --help\n"
            "      Display this help text.\n"
            "\n"
            "  -o filepath\n"
            "  --output filepath\n"
            "      Specify the output file name. By default, the output file has the same\n"
            "      name as the input and extension .bin (compile) or .xml (decompile). If\n"
            "      the specified path is a directory, the output file is built from this\n"
            "      directory and default file name. If more than one input file is specified,\n"
            "      the output path, if present, must be a directory name.\n"
            "\n"
            "  -v\n"
            "  --verbose\n"
            "      Produce verbose output.\n"
            "\n"
            "  --version\n"
            "      Display the version number.\n"
            "\n"
            "  -x\n"
            "  --xml-model\n"
            "      Display the XML model of the table files. This model is not a full\n"
            "      XML-Schema, this is an informal template file which describes the\n"
            "      expected syntax of TSDuck XML files. If --output is specified, save\n"
            "      the model here. Do not specify input files.\n");

    analyze(argc, argv);

    getValues(infiles, "");
    getValue(outfile, "output");
    compile = present("compile");
    decompile = present("decompile");
    xmlModel = present("xml-model");
    outdir = !outfile.empty() && ts::IsDirectory(outfile);

    if (present("verbose")) {
        setDebugLevel(ts::Severity::Verbose);
    }

    if (!infiles.empty() && xmlModel) {
        error("do not specify input files with --xml-model");
    }
    if (infiles.size() > 1 && !outfile.empty() && !outdir) {
        error("with more than one input file, --output must be a directory");
    }
    if (compile && decompile) {
        error("specify either --compile or --decompile but not both");
    }

    // Get default character set.
    const std::string csName(value("default-charset"));
    if (!csName.empty() && (defaultCharset = ts::DVBCharset::GetCharset(csName)) == 0) {
        error("invalid character set name '%s", csName.c_str());
    }

    exitOnError();
}


//----------------------------------------------------------------------------
//  Display the XML model.
//----------------------------------------------------------------------------

bool DisplayModel(Options& opt)
{
    // Locate the model file.
    const std::string inName(ts::SearchConfigurationFile("tsduck.xml"));
    if (inName.empty()) {
        opt.error("XML model file not found");
        return false;
    }
    opt.verbose("original model file is " + inName);

    // Save to a file. Default to stdout.
    std::string outName(opt.outfile);
    if (opt.outdir) {
        // Specified output is a directory, add default name.
        outName.push_back(ts::PathSeparator);
        outName.append("tsduck.xml");
    }
    if (!outName.empty()) {
        opt.verbose("saving model file to " + outName);
    }

    // Redirect input and output, exit in case of error.
    ts::InputRedirector in(inName, opt);
    ts::OutputRedirector out(outName, opt);

    // Display / copy the XML model.
    std::cout << std::cin.rdbuf();
    return true;
}


//----------------------------------------------------------------------------
//  Compile one source file. Return true on success, false on error.
//----------------------------------------------------------------------------

bool CompileXML(Options& opt, const std::string& infile, const std::string& outfile)
{
    opt.verbose("Compiling " + infile + " to " + outfile);
    ts::ReportWithPrefix report(opt, ts::BaseName(infile) + ": ");

    // Load XML file, convert tables to binary and save binary file.
    ts::XMLTables xml;
    return xml.loadXML(infile, report, opt.defaultCharset) && ts::BinaryTable::SaveFile(xml.tables(), outfile, report);
}


//----------------------------------------------------------------------------
//  Decompile one binary file. Return true on success, false on error.
//----------------------------------------------------------------------------

bool DecompileBinary(Options& opt, const std::string& infile, const std::string& outfile)
{
    opt.verbose("Decompiling " + infile + " to " + outfile);
    ts::ReportWithPrefix report(opt, ts::BaseName(infile) + ": ");

    // Load binary tables.
    ts::BinaryTablePtrVector tables;
    if (!ts::BinaryTable::LoadFile(tables, infile, ts::CRC32::CHECK, report)) {
        return false;
    }

    // Convert tables to XML and save XML file.
    ts::XMLTables xml;
    xml.add(tables);
    return xml.saveXML(outfile, report, opt.defaultCharset);
}


//----------------------------------------------------------------------------
//  Process one file. Return true on success, false on error.
//----------------------------------------------------------------------------

bool ProcessFile(Options& opt, const std::string& infile)
{
    const std::string ext(ts::LowerCaseValue(ts::PathSuffix(infile)));
    const bool isXML = ext == ".xml";
    const bool isBin = ext == ".bin";
    const bool compile = opt.compile || isXML;
    const bool decompile = opt.decompile || isBin;
    const char* const outExt = compile ? ".bin" : ".xml";

    // Compute output file name with default file type.
    std::string outname(opt.outfile);
    if (outname.empty()) {
        outname = ts::PathPrefix(infile) + outExt;
    }
    else if (opt.outdir) {
        outname += ts::PathSeparator + ts::PathPrefix(ts::BaseName(infile)) + outExt;
    }

    // Process the input file, starting with error cases.
    if (!compile && !decompile) {
        opt.error("don't know what to do with file " + infile + ", unknown file type, specify --compile or --decompile");
        return false;
    }
    else if (compile && isBin) {
        opt.error("cannot compile binary file " + infile);
        return false;
    }
    else if (decompile && isXML) {
        opt.error("cannot decompile XML file " + infile);
        return false;
    }
    else if (compile) {
        return CompileXML(opt, infile, outname);
    }
    else {
        return DecompileBinary(opt, infile, outname);
    }
}


//----------------------------------------------------------------------------
//  Program entry point
//----------------------------------------------------------------------------

int main(int argc, char *argv[])
{
    Options opt(argc, argv);
    bool ok = true;
    if (opt.xmlModel) {
        ok = DisplayModel(opt);
    }
    else {
        for (size_t i = 0; i < opt.infiles.size(); ++i) {
            if (!opt.infiles[i].empty()) {
                ok = ProcessFile(opt, opt.infiles[i]) && ok;
            }
        }
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
