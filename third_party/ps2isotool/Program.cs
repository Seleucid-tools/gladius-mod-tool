using Ps2IsoTools.UDF;
using Ps2IsoTools.UDF.Files;

// ── Usage ─────────────────────────────────────────────────────────────────────
//   ps2isotool extract <isoPath> <outDir>
//       Extracts all files from a PS2 UDF ISO into outDir, preserving the
//       on-disc directory structure.
//
//   ps2isotool build <srcDir> <outIso> [VOLNAME]
//       Builds a new PS2 UDF ISO from the files in srcDir.
//       VOLNAME defaults to GLADIUS (max 32 chars, A-Z 0-9 _).
// ─────────────────────────────────────────────────────────────────────────────

if (args.Length < 3)
{
    Console.Error.WriteLine("Usage:");
    Console.Error.WriteLine("  ps2isotool extract <isoPath> <outDir>");
    Console.Error.WriteLine("  ps2isotool build   <srcDir>  <outIso> [VOLNAME]");
    return 1;
}

string command = args[0].ToLowerInvariant();

// ── extract ───────────────────────────────────────────────────────────────────
if (command == "extract")
{
    string isoPath = args[1];
    string outDir  = args[2];

    if (!File.Exists(isoPath))
    {
        Console.Error.WriteLine($"ISO not found: {isoPath}");
        return 1;
    }

    Directory.CreateDirectory(outDir);

    using var reader = new UdfReader(isoPath);
    var files = reader.GetAllFileFullNames();

    int count = 0;
    foreach (string isoRelPath in files)
    {
        FileIdentifier? fi = reader.GetFileByName(isoRelPath);
        if (fi == null) continue;

        // isoRelPath uses backslashes on all platforms; convert to local separator
        string localRel = isoRelPath.Replace('\\', Path.DirectorySeparatorChar);
        string dest     = Path.Combine(outDir, localRel);
        string? destDir = Path.GetDirectoryName(dest);
        if (destDir != null) Directory.CreateDirectory(destDir);

        reader.CopyFile(fi, dest);
        Console.WriteLine($"  extracted: {isoRelPath}");
        count++;
    }

    Console.WriteLine($"Done. {count} file(s) extracted to: {outDir}");
    return 0;
}

// ── build ─────────────────────────────────────────────────────────────────────
if (command == "build")
{
    string srcDir  = args[1];
    string outIso  = args[2];
    string volName = args.Length > 3 ? args[3] : "GLADIUS";

    if (!Directory.Exists(srcDir))
    {
        Console.Error.WriteLine($"Source directory not found: {srcDir}");
        return 1;
    }

    var builder = new UdfBuilder { VolumeIdentifier = volName };

    // Register all directories first (AddDirectory accepts full paths like "DATA\CONFIG"
    // and creates intermediate directories automatically).
    var dirs = new SortedSet<string>(StringComparer.OrdinalIgnoreCase);
    string[] allFiles = Directory.GetFiles(srcDir, "*", SearchOption.AllDirectories);
    foreach (string filePath in allFiles)
    {
        string rel     = Path.GetRelativePath(srcDir, filePath);
        string isoRel  = rel.Replace(Path.DirectorySeparatorChar, '\\').ToUpperInvariant();
        string? dirPart = Path.GetDirectoryName(isoRel)?.Replace('/', '\\');
        if (!string.IsNullOrEmpty(dirPart)) dirs.Add(dirPart);
    }
    foreach (string dir in dirs)
        builder.AddDirectory(dir);

    // Add all files using full ISO-relative paths.
    int count = 0;
    foreach (string filePath in allFiles)
    {
        string rel    = Path.GetRelativePath(srcDir, filePath);
        string isoRel = rel.Replace(Path.DirectorySeparatorChar, '\\').ToUpperInvariant();
        Console.WriteLine($"  adding: {isoRel}");
        builder.AddFile(isoRel, filePath);
        count++;
    }

    builder.Build(outIso);
    Console.WriteLine($"Done. {count} file(s) packed into: {outIso}");
    return 0;
}

Console.Error.WriteLine($"Unknown command: {command}");
Console.Error.WriteLine("Commands: extract | build");
return 1;
