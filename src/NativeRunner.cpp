#include "NativeRunner.h"
#include "tools/TokNumUpdate.h"
#include "tools/UpdateStringsBin.h"
#include "tools/TokTool.h"
#include "tools/GladiusIdxUnpack.h"
#include "tools/GladiusIdxRepack.h"
#include "tools/NgcIsoTool.h"
#include "tools/BecTool.h"

#include <QFileInfo>

NativeRunner::NativeRunner(QObject *parent) : QObject(parent) {}

void NativeRunner::run(const QString &scriptName, const QStringList &args)
{
    // Accept either a bare script name or a full path — use only the filename.
    QString name = QFileInfo(scriptName).fileName();

    auto outCb = [this](const QString &msg) { emit output(msg); };
    auto errCb = [this](const QString &msg) { emit error(msg); };

    bool ok = false;

    // ── bec-tool ──────────────────────────────────────────────────────────────
    if (name == QStringLiteral("bec-tool")) {
        // Unpack: --platform <P> -unpack <becFile> <outDir>
        // Pack:   -pack <inDir> <outFile> <fileList> --platform <P>
        Tools::BecPlatform plat = Tools::BecPlatform::GC;
        int platIdx = args.indexOf(QStringLiteral("--platform"));
        if (platIdx >= 0 && platIdx + 1 < args.size()) {
            QString p = args[platIdx + 1];
            if (p == QStringLiteral("PS2"))  plat = Tools::BecPlatform::PS2;
            if (p == QStringLiteral("XBOX")) plat = Tools::BecPlatform::Xbox;
        }
        bool demoBec = args.contains(QStringLiteral("--demobec"));

        if (args.contains(QStringLiteral("-unpack"))) {
            int i = args.indexOf(QStringLiteral("-unpack"));
            if (i + 2 < args.size())
                ok = Tools::becUnpack(args[i+1], args[i+2], plat, demoBec, outCb, errCb);
            else
                errCb(QStringLiteral("bec-tool: -unpack requires <file> <outDir>"));
        } else if (args.contains(QStringLiteral("-pack"))) {
            int i = args.indexOf(QStringLiteral("-pack"));
            if (i + 3 < args.size())
                ok = Tools::becPack(args[i+1], args[i+2], args[i+3], plat, outCb, errCb);
            else
                errCb(QStringLiteral("bec-tool: -pack requires <inDir> <outFile> <fileList>"));
        } else {
            errCb(QStringLiteral("bec-tool: unknown mode"));
        }
    }

    // ── ngciso-tool ───────────────────────────────────────────────────────────
    else if (name == QStringLiteral("ngciso-tool")) {
        if (!args.isEmpty() && args[0] == QStringLiteral("-unpack")) {
            // -unpack <iso> <outDir> <fileList>
            if (args.size() >= 4)
                ok = Tools::ngcIsoUnpack(args[1], args[2], args[3], outCb, errCb);
            else
                errCb(QStringLiteral("ngciso-tool: -unpack requires <iso> <outDir> <fileList>"));
        } else if (!args.isEmpty() && args[0] == QStringLiteral("-pack")) {
            // -pack <inDir> <fstFile> <fstMap> <outIso>
            if (args.size() >= 5)
                ok = Tools::ngcIsoPack(args[1], args[2], args[3], args[4], outCb, errCb);
            else
                errCb(QStringLiteral("ngciso-tool: -pack requires <inDir> <fst> <map> <outIso>"));
        } else {
            errCb(QStringLiteral("ngciso-tool: unknown mode"));
        }
    }

    // ── idx-unpack ────────────────────────────────────────────────────────────
    else if (name == QStringLiteral("idx-unpack")) {
        if (!args.isEmpty())
            ok = Tools::idxUnpack(args[0], outCb, errCb);
        else
            errCb(QStringLiteral("IDX unpack: missing dataDir argument"));
    }

    // ── idx-repack ────────────────────────────────────────────────────────────
    else if (name == QStringLiteral("idx-repack")) {
        if (!args.isEmpty())
            ok = Tools::idxRepack(args[0], outCb, errCb);
        else
            errCb(QStringLiteral("IDX repack: missing dataDir argument"));
    }

    // ── tok-num-update ────────────────────────────────────────────────────────
    else if (name == QStringLiteral("tok-num-update")) {
        if (!args.isEmpty())
            ok = Tools::tokNumUpdate(args[0], outCb, errCb);
        else
            errCb(QStringLiteral("Tok_Num_Update: missing configDir argument"));
    }

    // ── tok-tool ──────────────────────────────────────────────────────────────
    else if (name == QStringLiteral("tok-tool")) {
        if (!args.isEmpty() && args[0] == QStringLiteral("-c") && args.size() >= 5) {
            ok = Tools::tokCompress(args[1], args[2], args[3], args[4], outCb, errCb);
        } else if (!args.isEmpty() && args[0] == QStringLiteral("-x") && args.size() >= 5) {
            ok = Tools::tokDecompress(args[1], args[2], args[3], args[4], outCb, errCb);
        } else {
            errCb(QStringLiteral("tok-tool: unknown mode or missing arguments"));
        }
    }

    // ── update-strings-bin ────────────────────────────────────────────────────
    else if (name == QStringLiteral("update-strings-bin")) {
        if (!args.isEmpty())
            ok = Tools::updateStringsBin(args[0], outCb, errCb);
        else
            errCb(QStringLiteral("Update_Strings_Bin: missing configDir argument"));
    }

    else {
        errCb(QStringLiteral("NativeRunner: unknown script: ") + name);
    }

    emit finished(ok);
}
