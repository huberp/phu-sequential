// Minimal call-graph extractor: method A -> methods X,Y,Z
// Uses Clang libTooling, outputs qualified names

#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>
#include <llvm/Support/raw_ostream.h>

#include <map>
#include <set>
#include <string>

using namespace clang;
using namespace clang::tooling;

std::map<std::string, std::set<std::string>> CG;
std::vector<std::string> FnStack;

static llvm::cl::OptionCategory MyToolCategory("cfg options");

std::string qname(Decl *D) {
  if (!D) return "";
  if (auto *ND = dyn_cast<NamedDecl>(D)) {
    std::string S;
    llvm::raw_string_ostream O(S);
    PrintingPolicy P(D->getASTContext().getLangOpts());
    P.SuppressScope = false;
    ND->printQualifiedName(O, P);
    return O.str();
  }
  return "";
}

class V : public RecursiveASTVisitor<V> {
public:
  bool TraverseFunctionDecl(FunctionDecl *FD) {
    std::string N = qname(FD);
    if (!N.empty()) FnStack.push_back(N);
    bool R = RecursiveASTVisitor<V>::TraverseFunctionDecl(FD);
    if (!N.empty() && !FnStack.empty() && FnStack.back() == N) FnStack.pop_back();
    return R;
  }
  bool TraverseCXXMethodDecl(CXXMethodDecl *MD) {
    std::string N = qname(MD);
    if (!N.empty()) FnStack.push_back(N);
    bool R = RecursiveASTVisitor<V>::TraverseCXXMethodDecl(MD);
    if (!N.empty() && !FnStack.empty() && FnStack.back() == N) FnStack.pop_back();
    return R;
  }
  bool VisitCallExpr(CallExpr *CE) {
    std::string C = FnStack.empty() ? "" : FnStack.back();
    if (C.empty()) return true;
    if (Decl *D = CE->getCalleeDecl()) {
      std::string T = qname(D);
      if (!T.empty()) CG[C].insert(T);
    }
    return true;
  }
  bool VisitCXXMemberCallExpr(CXXMemberCallExpr *CE) { return VisitCallExpr(CE); }
  bool VisitCXXOperatorCallExpr(CXXOperatorCallExpr *CE) { return VisitCallExpr(CE); }
};

class C : public ASTConsumer { V Visitor; public: void HandleTranslationUnit(ASTContext &Ctx) override { Visitor.TraverseDecl(Ctx.getTranslationUnitDecl()); } };
class A : public ASTFrontendAction { public: std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef) override { return std::make_unique<C>(); } };

int main(int argc, const char **argv) {
  CommonOptionsParser OP(argc, argv, MyToolCategory);
  ClangTool T(OP.getCompilations(), OP.getSourcePathList());
  if (T.run(newFrontendActionFactory<A>().get()) == 0) {
    for (const auto &E : CG) {
      llvm::outs() << E.first << " -> ";
      bool F = true;
      for (const auto &C : E.second) {
        if (!F) llvm::outs() << ", ";
        llvm::outs() << C;
        F = false;
      }
      llvm::outs() << "\n";
    }
  }
  return 0;
}
