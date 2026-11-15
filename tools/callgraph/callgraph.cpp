// Minimal call graph extractor using Clang libTooling
// Outputs: caller -> {callee1, callee2, ...} for all methods in /src and /lib

#include <clang/AST/ASTConsumer.h>
#include <clang/AST/RecursiveASTVisitor.h>
#include <clang/Frontend/CompilerInstance.h>
#include <clang/Frontend/FrontendAction.h>
#include <clang/Tooling/CommonOptionsParser.h>
#include <clang/Tooling/Tooling.h>
#include <llvm/Support/CommandLine.h>

#include <map>
#include <set>
#include <string>

using namespace clang;
using namespace clang::tooling;

// Global call graph: caller -> set of callees
std::map<std::string, std::set<std::string>> CallGraph;

// Track current function/method context
std::vector<std::string> CurrentFunctionStack;

static llvm::cl::OptionCategory MyToolCategory("callgraph options");

// Get qualified name for a Decl
std::string getQualifiedName(Decl *D) {
  if (!D) return "";
  
  if (auto *ND = dyn_cast<NamedDecl>(D)) {
    std::string Name;
    llvm::raw_string_ostream Stream(Name);
    PrintingPolicy Policy(D->getASTContext().getLangOpts());
    Policy.SuppressScope = false;
    Policy.SuppressTagKeyword = true;
    ND->printQualifiedName(Stream, Policy);
    return Stream.str();
  }
  return "";
}

// Get current function name
std::string getCurrentFunction() {
  if (CurrentFunctionStack.empty()) return "";
  return CurrentFunctionStack.back();
}

// AST Visitor
class CallGraphVisitor : public RecursiveASTVisitor<CallGraphVisitor> {
public:
  explicit CallGraphVisitor(ASTContext *Context) : Context(Context) {}
  
  // Track function/method entry and exit via Traverse only
  bool TraverseFunctionDecl(FunctionDecl *FD) {
    std::string Name = getQualifiedName(FD);
    if (!Name.empty()) {
      CurrentFunctionStack.push_back(Name);
    }
    bool Result = RecursiveASTVisitor<CallGraphVisitor>::TraverseFunctionDecl(FD);
    if (!Name.empty() && !CurrentFunctionStack.empty() && CurrentFunctionStack.back() == Name) {
      CurrentFunctionStack.pop_back();
    }
    return Result;
  }
  
  bool TraverseCXXMethodDecl(CXXMethodDecl *MD) {
    std::string Name = getQualifiedName(MD);
    if (!Name.empty()) {
      CurrentFunctionStack.push_back(Name);
    }
    bool Result = RecursiveASTVisitor<CallGraphVisitor>::TraverseCXXMethodDecl(MD);
    if (!Name.empty() && !CurrentFunctionStack.empty() && CurrentFunctionStack.back() == Name) {
      CurrentFunctionStack.pop_back();
    }
    return Result;
  }
  
  // Visit all call expressions
  bool VisitCallExpr(CallExpr *CE) {
    std::string Caller = getCurrentFunction();
    if (Caller.empty()) return true;
    
    Decl *Callee = CE->getCalleeDecl();
    if (!Callee) return true; // Skip indirect calls
    
    std::string CalleeName = getQualifiedName(Callee);
    if (!CalleeName.empty()) {
      CallGraph[Caller].insert(CalleeName);
    }
    
    return true;
  }
  
  bool VisitCXXMemberCallExpr(CXXMemberCallExpr *CE) { return VisitCallExpr(CE); }
  
  bool VisitCXXOperatorCallExpr(CXXOperatorCallExpr *CE) { return VisitCallExpr(CE); }

private:
  ASTContext *Context;
};

// AST Consumer
class CallGraphConsumer : public ASTConsumer {
public:
  explicit CallGraphConsumer(ASTContext *Context) : Visitor(Context) {}
  
  void HandleTranslationUnit(ASTContext &Context) override {
    Visitor.TraverseDecl(Context.getTranslationUnitDecl());
  }

private:
  CallGraphVisitor Visitor;
};

// Frontend Action
class CallGraphAction : public ASTFrontendAction {
public:
  std::unique_ptr<ASTConsumer> CreateASTConsumer(CompilerInstance &CI, StringRef file) override {
    return std::make_unique<CallGraphConsumer>(&CI.getASTContext());
  }
};

// Output the call graph
void outputCallGraph() {
  for (const auto &Entry : CallGraph) {
    llvm::outs() << Entry.first << " -> ";
    bool First = true;
    for (const auto &Callee : Entry.second) {
      if (!First) llvm::outs() << ", ";
      llvm::outs() << Callee;
      First = false;
    }
    llvm::outs() << "\n";
  }
}

int main(int argc, const char **argv) {
  CommonOptionsParser OptionsParser(argc, argv, MyToolCategory);
  ClangTool Tool(OptionsParser.getCompilations(), OptionsParser.getSourcePathList());
  
  int Result = Tool.run(newFrontendActionFactory<CallGraphAction>().get());
  
  if (Result == 0) {
    outputCallGraph();
  }
  
  return Result;
}
