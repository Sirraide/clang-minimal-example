#include <vector>
#include <string>
#include <clang/Tooling/Tooling.h>   // for clang::tooling::buildASTFromCodeWithArgs
#include <clang/Frontend/ASTUnit.h>  // for clang::ASTUnit
#include <clang/AST/ASTContext.h>    // for clang::ASTContext
#include <clang/AST/Decl.h>          // for clang::TranslationUnitDecl

using namespace clang;
int main([[maybe_unused]] int argc, char** argv) {
    std::vector<std::string> Args {
        "-std=c++20",
        "-Wall",
    };

    // This parses the code and hands us back an 'ASTUnit', which is
    // the parsed AST together with all of the other state that was
    // used during parsing (e.g. the Preprocessor, the Sema instance,
    // etc.)
    std::unique_ptr<ASTUnit> AST = tooling::buildASTFromCodeWithArgs(
        argv[1],
        Args,
        "input.cc",
        CLANG_PATH
    );

    // Something went horribly wrong, bail out.
    if (!AST) return 1;

    // We usually still have a somewhat-valid AST even if there was
    // an error (which will have already been printed), but for this
    // example, we also bail out if there was a compile error.
    if (AST->getDiagnostics().hasUncompilableErrorOccurred()) return 1;

    // Dump the entire AST.
    AST->getASTContext().getTranslationUnitDecl()->dumpColor();
}
