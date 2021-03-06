#include "Function.h"
#include "Struct.h"
#include "Comments.h"
#include "StringEx.h"
#include <sstream>
#include "GameVersions.h"

const size_t REFS_LIST_MAX_SIZE = 100;

string Function::GetFullName() const {
    if (mScope.empty())
        return mName;
    return mScope + "::" + mName;
}

string Function::NameForWrapper(Games::IDs game, bool definition, string const &customName, bool wsFuncs) {
    string result;
    if (!definition) {
        result += GameVersions::GetSupportedGameVersionsMacro(game, mVersionInfo) + " ";
        if (mIsStatic)
            result += "static ";
    }
    if (!IsConstructor() && !IsDestructor()) {
        if (mRVOParamIndex != -1)
            result += mParameters[mRVOParamIndex].mType.GetFullTypeRemovePointer();
        else
            result += mRetType.GetFullType();
    }
    if (!customName.empty())
        result += customName;
    else {
        if (!definition)
            result += mName;
        else
            result += GetFullName();
    }
    result += "(";
    IterateIndex(mParameters, [&](FunctionParameter &p, unsigned int index) {
        if (index != 0)
            result += ", ";
        if (wsFuncs)
            p.mType.mName = "char";
        result += p.mType.BeforeName() + p.mName + p.mType.AfterName();
        if (wsFuncs)
            p.mType.mName = "wchar_t";
        if (!definition && !p.mDefValue.empty())
            result += " = " + p.mDefValue;
    }, mNumParamsToSkipForWrapper);
    result += ")";
    return result;
}

void Function::WriteFunctionCall(ofstream &stream, tabs t, Games::IDs game, bool writeReturn, SpecialCall specialType,
    SpecialData specialData, bool wsFuncs)
{
    bool noReturn = (mRetType.mIsVoid && mRetType.mPointers.empty()) || IsConstructor() || IsDestructor();
    if (mRVOParamIndex != -1) {
        FunctionParameter &retParam = mParameters[mRVOParamIndex];
        stream << t() << retParam.mType.GetFullTypeRemovePointer() << retParam.mName << ";" << endl;
        noReturn = true;
    }
    stream << t();
    if (!noReturn && writeReturn)
        stream << "return ";
    stream << "plugin::Call";
    if (mIsVirtual)
        stream << "Virtual";
    if (mCC == CC_THISCALL)
        stream << "Method";
    if (!noReturn)
        stream << "AndReturn";
    if (!mIsVirtual)
        stream << "DynGlobal";
    if (mParameters.size() > 0 || !noReturn || mIsVirtual) {
        stream << "<";
        if (!noReturn) {
            stream << mRetType.GetFullType(false);
            if (mIsVirtual)
                stream << ", " << mVTableIndex;
        }
        else if (mIsVirtual)
            stream << mVTableIndex;
        IterateFirstLast(mParameters, [&](FunctionParameter &p, bool first, bool last) {
            if (!first || !noReturn || mIsVirtual)
                stream << ", ";
            if (mClass && !mIsStatic && first)
                stream << mShortClassName << " *";
            else
                stream << p.mType.GetFullType(false);
        });
        stream << ">(";
    }
    else
        stream << "(";
    if (!mIsVirtual)
        stream << AddrOfMacro(true);
    IterateIndex(mParameters, [&](FunctionParameter &p, unsigned int index) {
        if (!mIsVirtual || index != 0)
            stream << ", ";
        if (index == 0 && specialType == SpecialCall::StackObject)
            stream << "reinterpret_cast<" << mFullClassName << " *>(objBuff)";
        else if (index == 0 && specialType == SpecialCall::Custom_OperatorNew)
            stream << "sizeof(" << specialData.mClassNameForOpNewDelete << ")";
        else if (index == 0 && specialType == SpecialCall::Custom_Array_OperatorNew)
            stream << "sizeof(" << specialData.mClassNameForOpNewDelete << ") * objCount + 4";
        else if (index == 0 && (specialType == SpecialCall::Custom_Constructor ||
            specialType == SpecialCall::Custom_DeletingDestructor || specialType == SpecialCall::Custom_BaseDestructor ||
            specialType == SpecialCall::Custom_OperatorDelete))
        {
            stream << "obj";
        }
        else if (index == 0 && (specialType == SpecialCall::Custom_Array_DeletingArrayDestructor
            || specialType == SpecialCall::Custom_Array_OperatorDelete))
        {
            stream << "objArray";
        }
        else if (index == 0 && (specialType == SpecialCall::Custom_Array_BaseDestructor ||
            specialType == SpecialCall::Custom_Array_DeletingDestructor || specialType == SpecialCall::Custom_Array_Constructor))
        {
            stream << "&objArray[i]";
        }
        else if (index == 1 && (specialType == SpecialCall::Custom_DeletingDestructor ||
            specialType == SpecialCall::Custom_Array_DeletingDestructor))
        {
            stream << "1";
        }
        else if (index == 1 && specialType == SpecialCall::Custom_Array_DeletingArrayDestructor)
            stream << "3";
        else  if (index == 0 && mClass && !mIsStatic)
            stream << "this";
        else {
            if (index == mRVOParamIndex)
                stream << "&";
            stream << p.mName;
        }
    });
    stream << ");" << endl;
    if (writeReturn && mRVOParamIndex != -1)
        stream << t() << "return " << mParameters[mRVOParamIndex].mName << ";" << endl;
}

void Function::WriteDefinition(ofstream &stream, tabs t, Games::IDs game, Flags flags) {
    if (flags.Empty()) {
        stream << t() << "int " << AddrOfMacro(false) << " = ADDRESS_BY_VERSION(" << Addresses(game) << ");" << endl;
        stream << t() << "int " << AddrOfMacro(true) << " = GLOBAL_ADDRESS_BY_VERSION(" << Addresses(game) << ");";
    }
    if (mClass && mClass->UsesCustomConstruction() && (IsConstructor() || IsDestructor() || IsOperatorNewDelete()))
        return;
    if (flags.Empty())
        stream << endl << endl;
    stream << t() << NameForWrapper(game, true, string(), flags.OverloadedWideStringFunc) << " {" << endl;
    ++t;
    WriteFunctionCall(stream, t, game, true, Function::SpecialCall::None, SpecialData(), flags.OverloadedWideStringFunc);
    --t;
    stream << t() << "}";
}

void Function::WriteDeclaration(ofstream &stream, tabs t, Games::IDs game, Flags flags) {
    WriteComment(stream, mComment, t, 0);
    stream << t() << NameForWrapper(game, false, string(), flags.OverloadedWideStringFunc) << ";";
}

void Function::WriteMeta(ofstream &stream, tabs t, Games::IDs game) {
    stream << t() << String::ToUpper(GetSpecialMetaWord()) << "META_BEGIN";
    if (UsesOverloadedMetaMacro())
        stream << "_OVERLOADED";
    stream << "(" << MetaDesc() << ")" << endl;
    stream << t() << "static int address;" << endl;
    stream << t() << "static int global_address;" << endl;
    stream << t() << "static const int id = " << String::ToHexString(mVersionInfo[0].mAddress) << ";" << endl;
    stream << t() << "static const bool is_virtual = " << (mIsVirtual ? "true" : "false") << ";" << endl;
    stream << t() << "static const int vtable_index = " << mVTableIndex << ";" << endl;
    stream << t() << "using mv_addresses_t = MvAddresses<" << Addresses(game) << ">;" << endl;
    stream << t() << "// total references count: ";
    List<FunctionReference> refs;
    for (unsigned int i = 0; i < Games::GetGameVersionsCount(game); i++) {
        std::istringstream ss(mVersionInfo[i].mRefsStr);
        std::string token;
        unsigned int refsCount = 0;
        while (ss >> token) {
            FunctionReference ref;
            ref.mGameVersion = i;
            ref.mRefAddr = String::ToNumber(token);
            ss >> token;
            ref.mRefType = String::ToNumber(token);
            ss >> token;
            ref.mRefObjectId = String::ToNumber(token);
            ss >> token;
            ref.mRefIndexInObject = String::ToNumber(token);
            refs.push_back(ref);
            refsCount++;
        }
        if (i != 0)
            stream << ", ";
        stream << Games::GetGameVersionName(game, i) << " (" << refsCount << ")";
    }
    stream << endl;
    //if (refs.size() > REFS_LIST_MAX_SIZE) {
    //    stream << t() << "// NOTE: too much references; ignored" << std::endl;
    //    stream << t() << "// using refs_t = RefList<>;" << std::endl;
    //}
    //else {
        stream << t() << "using refs_t = RefList<";
        if (refs.size() > 0) {
            IterateFirstLast(refs, [&](FunctionReference &ref, bool first, bool last) {
                if (!first)
                    stream << " ";
                stream << String::ToHexString(ref.mRefAddr) << ",";
                stream << Games::GetUniqueId(game, ref.mGameVersion);
                stream << "," << ref.mRefType << ",";
                stream << String::ToHexString(ref.mRefObjectId) << ",";
                stream << ref.mRefIndexInObject;
                if (!last)
                    stream << ",";
            });
        }
        stream << ">;" << endl;
    //}
    stream << t() << "using def_t = " << mRetType.GetFullType(false) << "(";
    IterateIndex(mParameters, [&](FunctionParameter &p, unsigned int index) {
        if (index != 0)
            stream << ", ";
        stream << p.mType.GetFullType(false);
    });
    stream << ");" << endl;
    stream << t() << "static const int cb_priority = PRIORITY_";
    if (mPriority == 1)
        stream << "AFTER";
    else
        stream << "BEFORE";
    stream << "; " << endl;
    stream << t() << "using calling_convention_t = CallingConventions::";
    if (mCC == Function::CC_CDECL)
        stream << "Cdecl";
    else if (mCC == Function::CC_THISCALL)
        stream << "Thiscall";
    stream << ";" << endl;
    stream << t() << "using args_t = ArgPick<ArgTypes<";
    IterateIndex(mParameters, [&](FunctionParameter &p, unsigned int index) {
        if (index != 0)
            stream << ",";
        stream << p.mType.GetFullType(false);
    });
    stream << ">";
    IterateIndex(mParameters, [&](FunctionParameter &p, unsigned int index) {
        if (index == 0)
            stream << ", ";
        else
            stream << ",";
        stream << index;
    });
    stream << ">;" << endl;
    stream << t() << "META_END";
}

string Function::MetaDesc() {
    string result;
    if (mUsage == Usage::Default || mUsage == Usage::Operator)
        result = GetFullName();
    else
        result = mFullClassName;
    if (UsesOverloadedMetaMacro()) {
        result += ", ";
        if (mUsage == Usage::DefaultConstructor || mUsage == Usage::CustomConstructor || mUsage == Usage::CopyConstructor)
            result += "void";
        else
            result += mRetType.GetFullType();
        result += "(";
        if (mUsage == Usage::Default || mUsage == Usage::Operator) {
            if (mCC == CC_THISCALL)
                result += mScope + "::";
            result += "*)(";
        }
        IterateIndex(mParameters, [&](FunctionParameter &p, unsigned int index) {
            if (index != 0)
                result += ", ";
            result += p.mType.GetFullType(false);
        }, mNumParamsToSkipForWrapper);
        result += ")";
    }
    return result;
}

bool Function::UsesOverloadedMetaMacro() {
    if (mUsage == Usage::DefaultConstructor ||
        mUsage == Usage::DefaultOperatorNew ||
        mUsage == Usage::DefaultOperatorNewArray ||
        mUsage == Usage::DefaultOperatorDelete ||
        mUsage == Usage::DefaultOperatorDeleteArray
        || IsDestructor())
    {
        return false;
    }
    if (mUsage == Usage::CustomConstructor ||
        mUsage == Usage::CopyConstructor ||
        mUsage == Usage::CustomOperatorNew ||
        mUsage == Usage::CustomOperatorNewArray ||
        mUsage == Usage::CustomOperatorDelete ||
        mUsage == Usage::CustomOperatorDeleteArray)
    {
        return true;
    }
    return mIsOverloaded || mForceOverloadedMetaMacro;
}

string Function::GetSpecialMetaWord() {
    if (mUsage == Usage::DefaultConstructor || mUsage == Usage::CustomConstructor || mUsage == Usage::CopyConstructor)
        return "ctor_";
    if (mUsage == Usage::BaseDestructor)
        return "dtor_";
    if (mUsage == Usage::DeletingDestructor)
        return "del_dtor_";
    if (mUsage == Usage::DefaultOperatorNew || mUsage == Usage::CustomOperatorNew)
        return "op_new_";
    if (mUsage == Usage::DefaultOperatorNewArray || mUsage == Usage::CustomOperatorNewArray)
        return "op_new_array_";
    if (mUsage == Usage::DefaultOperatorDelete || mUsage == Usage::CustomOperatorDelete)
        return "op_delete_";
    if (mUsage == Usage::DefaultOperatorDeleteArray || mUsage == Usage::CustomOperatorDeleteArray)
        return "op_delete_array_";
    return "";
}

string Function::AddrOfMacro(bool global) {
    string specialMetaWord = GetSpecialMetaWord();
    string result = specialMetaWord;
    if (global)
        result += "g";
    result += "addr";
    if (specialMetaWord.empty())
        result += "of";
    if (UsesOverloadedMetaMacro())
        result += "_o";
    result += "(" + MetaDesc() + ")";
    return result;
}

string Function::Addresses(Games::IDs game) {
    string result;
    bool first = true;
    for (unsigned int i = 0; i < Games::GetGameVersionsCount(game); i++) {
        if (game == Games::IDs::GTASA && i == 1) // skip GTASA 1.0 US HoodLum
            continue;
        if (!first)
            result += ", ";
        else
            first = false;
        result += String::ToHexString(mVersionInfo[i].mAddress);
    }
    return result;
}

bool Function::HasDefaultOpNewParams() {
    return mParameters.size() == 1 &&
        (mParameters[0].mType.mName == "int"
            || mParameters[0].mType.mName == "unsigned int"
            || mParameters[0].mType.mName == "size_t"
            || mParameters[0].mType.mName == "long"
            || mParameters[0].mType.mName == "unsigned long");
}

bool Function::HasDefaultOpDeleteParams() {
    return mParameters.size() == 1 && mParameters[0].mType.mName == "void" && mParameters[0].mType.mPointers.size() == 1 &&
        mParameters[0].mType.mPointers[0] == '*';
}

bool Function::IsDestructor() {
    return mUsage == Function::Usage::BaseDestructor || mUsage == Function::Usage::DeletingDestructor;
}

bool Function::IsConstructor() {
    return mUsage == Function::Usage::DefaultConstructor || mUsage == Function::Usage::CustomConstructor
        || mUsage == Usage::CopyConstructor;
}

bool Function::IsOperatorNew() {
    return mUsage == Function::Usage::DefaultOperatorNew || mUsage == Function::Usage::CustomOperatorNew
        || mUsage == Function::Usage::DefaultOperatorNewArray || mUsage == Function::Usage::CustomOperatorNewArray;
}

bool Function::IsOperatorDelete() {
    return mUsage == Function::Usage::DefaultOperatorDelete || mUsage == Function::Usage::CustomOperatorDelete
        || mUsage == Function::Usage::DefaultOperatorDeleteArray || mUsage == Function::Usage::CustomOperatorDeleteArray;
}

bool Function::IsOperatorNewDelete() {
    return IsOperatorNew() || IsOperatorDelete();
}
