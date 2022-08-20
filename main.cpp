#include "CmdlineParser.h"
#include "csxfile.h"

void usage() {
    printf ("Usage: cotodec --export --input /path/to/csx/file/name.csx --output /absolute/path/to/output/folder");
}

int main(int argc, char *argv[])
{
    usage();
    bool export_csx, imp;
    std::string input, output;

    CmdlineParser cmds(argc, argv);
    cmds.add_bool_param("export", &export_csx, false, "Export data from archive");
    cmds.add_bool_param("import", &imp, false, "");
    cmds.add_string_param("input", &input, "", "Input file (directory)");
    cmds.add_string_param("output", &output, "", "Output directory (file)");
    cmds.parse();

    CSXFile* csx = new CSXFile();

    if (export_csx)
    {
        csx->load_from_file(input);

        csx->read_offsets();
        //csx->print_offsets();

        csx->read_conststr();
        //csx->print_conststr();

        csx->read_linkinf();

        csx->read_global();
        csx->print_global();

        csx->export_all_sections(output);

        csx->decompile();
        csx->listing_to_file(output);
    }

    delete csx;

    return 0;
}
