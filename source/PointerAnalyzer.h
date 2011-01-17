#ifndef __BOA_POINTERANALYZER_H
#define __BOA_POINTERANALYZER_H

#include "clang/Frontend/FrontendPluginRegistry.h"
#include "clang/AST/ASTConsumer.h"
#include "clang/AST/ASTContext.h"
#include "clang/AST/AST.h"
#include "clang/AST/RecursiveASTVisitor.h"
#include "clang/Basic/SourceManager.h"
#include "clang/Frontend/CompilerInstance.h"
#include "llvm/Support/raw_ostream.h"

#include "Buffer.h"
#include "ConstraintProblem.h"
#include "Pointer.h"

#include "log.h"

#include <vector>
using std::vector;
#include <map>
using std::map;

using namespace clang;

namespace boa {

class PointerASTVisitor : public RecursiveASTVisitor<PointerASTVisitor> {
private:
  vector<Buffer> Buffers_;
  vector<Pointer> Pointers_;
  map< Pointer, vector<Buffer>* > Pointer2Buffers_;
  SourceManager &sm_;
public:
  PointerASTVisitor(SourceManager &SM)
    : sm_(SM) {}

  bool VisitStmt(Stmt* S) {
    if (DeclStmt* dec = dyn_cast<DeclStmt>(S)) {
      for (DeclGroupRef::iterator i = dec->decl_begin(), end = dec->decl_end(); i != end; ++i) {
        findVarDecl(*i);
      }
    } else if (CallExpr* funcCall = dyn_cast<CallExpr>(S)) {
      if (FunctionDecl* funcDec = funcCall->getDirectCallee())
      {
         if (funcDec->getNameInfo().getAsString() == "malloc")
         {
            addMallocToSet(funcCall,funcDec);
         }
      }
    } else if (StringLiteral* stringLiteral = dyn_cast<StringLiteral>(S)) {
      addConstStringToSet(stringLiteral);
    }
    return true;
  }

  bool VisitDecl(Decl* D) {
    if (FunctionDecl* func = dyn_cast<FunctionDecl>(D)) {
      for (unsigned i = 0; i < func->param_size(); ++i) {
        findVarDecl(func->getParamDecl(i));
      }
      if (func->hasBody()) {
        VisitStmt(func->getBody());
      }
    }
    return true;
  }


  void findVarDecl(Decl *d) {
    if (VarDecl* var = dyn_cast<VarDecl>(d)) {
      Type* varType = var->getType().getTypePtr();
      // FIXME - This code only detects an array of chars
      // Array of array of chars will probably NOT detected
      if (ArrayType* arr = dyn_cast<ArrayType>(varType)) {
        if (arr->getElementType().getTypePtr()->isAnyCharacterType()) {
          addBufferToSet(var);
        }
      } else if (PointerType* pType = dyn_cast<PointerType>(varType)) {
        if (pType->getPointeeType()->isAnyCharacterType()) {
           addPointerToSet(var);
        }
      }
    }
  }

  void addBufferToSet(VarDecl* var) {
    Buffer b((void*)var, var->getNameAsString(), sm_.getBufferName(var->getLocation()),
              sm_.getSpellingLineNumber(var->getLocation()));
    Buffers_.push_back(b);
    LOG << "Adding buffer - " << b.getReadableName() << " at " <<  b.getSourceLocation() << endl;
  }

  void addMallocToSet(CallExpr* funcCall, FunctionDecl* func) {
    Buffer b((void*)funcCall, "MALLOC", sm_.getBufferName(funcCall->getLocStart()),
              sm_.getSpellingLineNumber(funcCall->getLocStart()));
    Buffers_.push_back(b);
    LOG << "Adding malloc buffer at " + b.getSourceLocation() << endl;
  }

  void addPointerToSet(VarDecl* var) {
    Pointer p((void*)var);
    Pointers_.push_back(p);
    Pointer2Buffers_[p] = &Buffers_;
    LOG << "Adding pointer - " << var->getNameAsString() << " at line " <<
        sm_.getSpellingLineNumber(var->getLocation()) << endl;
  }

  void addConstStringToSet(StringLiteral* stringLiteral) {
    Buffer b((void*)stringLiteral, "Literal", sm_.getBufferName(stringLiteral->getLocStart()),
              sm_.getSpellingLineNumber(stringLiteral->getLocStart()));
    Buffers_.push_back(b);
    log::os() << "Adding string literal buffer: " + stringLiteral->getString().str() << endl;
  }

  const vector<Buffer>& getBuffers() const {
    return Buffers_;
  }

  const vector<Pointer>& getPointers() const {
    return Pointers_;
  }

  const map< Pointer, vector<Buffer>* > & getMapping() const {
    return Pointer2Buffers_;
  }
};

}  // namespace boa

#endif  // __BOA_POINTERANALYZER_H
