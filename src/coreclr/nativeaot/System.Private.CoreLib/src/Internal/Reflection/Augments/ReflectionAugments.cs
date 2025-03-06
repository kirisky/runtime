// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

//  Internal.Reflection.Augments
//  -------------------------------------------------
//  Why does this exist?:
//    Also, IntrospectionServices.GetTypeInfo() and Assembly.Load()
//    are defined in System.Reflection but need a way to "call into"
//    Reflection.Core.dll to do the real work.
//
//    This contract adds the additional entrypoints needed to System.Reflection.
//
//  Implemented by:
//    System.Reflection.dll on RH (may use ILMerging instead)
//
//  Consumed by:
//    Reflection.Core.dll

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Globalization;
using System.Reflection;
using System.Reflection.Runtime.Assemblies;
using System.Reflection.Runtime.BindingFlagSupport;
using System.Reflection.Runtime.FieldInfos;
using System.Reflection.Runtime.FieldInfos.NativeFormat;
using System.Reflection.Runtime.General;
using System.Reflection.Runtime.MethodInfos;
using System.Reflection.Runtime.TypeInfos;
using System.Reflection.Runtime.TypeInfos.NativeFormat;

using Internal.Metadata.NativeFormat;
using Internal.Reflection.Core.Execution;
using Internal.Runtime;
using Internal.Runtime.Augments;

using EETypeElementType = Internal.Runtime.EETypeElementType;

namespace Internal.Reflection.Augments
{
    internal static class ReflectionAugments
    {
        internal static unsafe TypeCode GetRuntimeTypeCode(RuntimeType type)
        {
            Debug.Assert(type != null);

            MethodTable* eeType = type.ToMethodTableMayBeNull();
            if (eeType == null)
            {
                // Type exists in metadata only. Aside from the enums, there is no chance a type with a TypeCode would not have an MethodTable,
                // so if it's not an enum, return the default.
                if (!type.IsActualEnum)
                    return TypeCode.Object;
                Type underlyingType = Enum.GetUnderlyingType(type);
                eeType = underlyingType.TypeHandle.ToMethodTable();
            }

            // Note: Type.GetTypeCode() is expected to return the underlying type's TypeCode for enums. EETypePtr.CorElementType does the same,
            // so this one switch handles both cases.
            EETypeElementType rhType = eeType->ElementType;
            switch (rhType)
            {
                case EETypeElementType.Boolean: return TypeCode.Boolean;
                case EETypeElementType.Char: return TypeCode.Char;
                case EETypeElementType.SByte: return TypeCode.SByte;
                case EETypeElementType.Byte: return TypeCode.Byte;
                case EETypeElementType.Int16: return TypeCode.Int16;
                case EETypeElementType.UInt16: return TypeCode.UInt16;
                case EETypeElementType.Int32: return TypeCode.Int32;
                case EETypeElementType.UInt32: return TypeCode.UInt32;
                case EETypeElementType.Int64: return TypeCode.Int64;
                case EETypeElementType.UInt64: return TypeCode.UInt64;
                case EETypeElementType.Single: return TypeCode.Single;
                case EETypeElementType.Double: return TypeCode.Double;
                default:
                    break;
            }

            if (type == typeof(string))
                return TypeCode.String;

            if (type == typeof(DateTime))
                return TypeCode.DateTime;

            if (type == typeof(decimal))
                return TypeCode.Decimal;

            if (type == typeof(DBNull))
                return TypeCode.DBNull;

            return TypeCode.Object;
        }

        public static Assembly Load(AssemblyName assemblyRef, bool throwOnFileNotFound)
        {
            ArgumentNullException.ThrowIfNull(assemblyRef);

            if (throwOnFileNotFound)
                return RuntimeAssemblyInfo.GetRuntimeAssembly(assemblyRef.ToRuntimeAssemblyName());
            else
                return RuntimeAssemblyInfo.GetRuntimeAssemblyIfExists(assemblyRef.ToRuntimeAssemblyName());
        }

        public static Assembly Load(ReadOnlySpan<byte> rawAssembly, ReadOnlySpan<byte> pdbSymbolStore)
        {
            if (rawAssembly.IsEmpty)
                throw new ArgumentNullException(nameof(rawAssembly));

            return RuntimeAssemblyInfo.GetRuntimeAssemblyFromByteArray(rawAssembly, pdbSymbolStore);
        }

        public static Assembly Load(string assemblyPath)
        {
            ArgumentNullException.ThrowIfNull(assemblyPath);

            return RuntimeAssemblyInfo.GetRuntimeAssemblyFromPath(assemblyPath);
        }

        //
        // This overload of GetMethodForHandle only accepts handles for methods declared on non-generic types (the method, however,
        // can be an instance of a generic method.) To resolve handles for methods declared on generic types, you must pass
        // the declaring type explicitly using the two-argument overload of GetMethodFromHandle.
        //
        // This is a vestige from desktop generic sharing that got itself enshrined in the code generated by the C# compiler for Linq Expressions.
        //
        public static MethodBase GetMethodFromHandle(RuntimeMethodHandle runtimeMethodHandle)
        {
            ExecutionEnvironment executionEnvironment = ReflectionCoreExecution.ExecutionEnvironment;
            QMethodDefinition methodHandle;
            RuntimeTypeHandle declaringTypeHandle;
            RuntimeTypeHandle[] genericMethodTypeArgumentHandles;
            if (!executionEnvironment.TryGetMethodFromHandle(runtimeMethodHandle, out declaringTypeHandle, out methodHandle, out genericMethodTypeArgumentHandles))
                throw new ArgumentException(SR.Argument_InvalidHandle);

            MethodBase methodBase = ExecutionDomain.GetMethod(declaringTypeHandle, methodHandle, genericMethodTypeArgumentHandles);
            if (methodBase.DeclaringType.IsConstructedGenericType)  // For compat with desktop, insist that the caller pass us the declaring type to resolve members of generic types.
                throw new ArgumentException(SR.Format(SR.Argument_MethodDeclaringTypeGeneric, methodBase, methodBase.DeclaringType.GetGenericTypeDefinition()));
            return methodBase;
        }

        //
        // This overload of GetMethodHandle can handle all method handles.
        //
        public static MethodBase GetMethodFromHandle(RuntimeMethodHandle runtimeMethodHandle, RuntimeTypeHandle declaringTypeHandle)
        {
            ExecutionEnvironment executionEnvironment = ReflectionCoreExecution.ExecutionEnvironment;
            QMethodDefinition methodHandle;
            RuntimeTypeHandle[] genericMethodTypeArgumentHandles;
            if (!executionEnvironment.TryGetMethodFromHandleAndType(runtimeMethodHandle, declaringTypeHandle, out methodHandle, out genericMethodTypeArgumentHandles))
            {
                // This may be a method declared on a non-generic type: this api accepts that too so try the other table.
                RuntimeTypeHandle actualDeclaringTypeHandle;
                if (!executionEnvironment.TryGetMethodFromHandle(runtimeMethodHandle, out actualDeclaringTypeHandle, out methodHandle, out genericMethodTypeArgumentHandles))
                    throw new ArgumentException(SR.Argument_InvalidHandle);
                if (!actualDeclaringTypeHandle.Equals(declaringTypeHandle))
                    throw new ArgumentException(SR.Format(SR.Argument_ResolveMethodHandle,
                        declaringTypeHandle.GetRuntimeTypeInfoForRuntimeTypeHandle(),
                        actualDeclaringTypeHandle.GetRuntimeTypeInfoForRuntimeTypeHandle()));
            }

            MethodBase methodBase = ExecutionDomain.GetMethod(declaringTypeHandle, methodHandle, genericMethodTypeArgumentHandles);
            return methodBase;
        }

        //
        // This overload of GetFieldForHandle only accepts handles for fields declared on non-generic types. To resolve handles for fields
        // declared on generic types, you must pass the declaring type explicitly using the two-argument overload of GetFieldFromHandle.
        //
        // This is a vestige from desktop generic sharing that got itself enshrined in the code generated by the C# compiler for Linq Expressions.
        //
        public static FieldInfo GetFieldFromHandle(RuntimeFieldHandle runtimeFieldHandle)
        {
            ExecutionEnvironment executionEnvironment = ReflectionCoreExecution.ExecutionEnvironment;
            FieldHandle fieldHandle;
            RuntimeTypeHandle declaringTypeHandle;
            if (!executionEnvironment.TryGetFieldFromHandle(runtimeFieldHandle, out declaringTypeHandle, out fieldHandle))
                throw new ArgumentException(SR.Argument_InvalidHandle);

            FieldInfo fieldInfo = GetFieldInfo(declaringTypeHandle, fieldHandle);
            if (fieldInfo.DeclaringType.IsConstructedGenericType) // For compat with desktop, insist that the caller pass us the declaring type to resolve members of generic types.
                throw new ArgumentException(SR.Format(SR.Argument_FieldDeclaringTypeGeneric, fieldInfo));
            return fieldInfo;
        }

        //
        // This overload of GetFieldHandle can handle all field handles.
        //
        public static FieldInfo GetFieldFromHandle(RuntimeFieldHandle runtimeFieldHandle, RuntimeTypeHandle declaringTypeHandle)
        {
            ExecutionEnvironment executionEnvironment = ReflectionCoreExecution.ExecutionEnvironment;
            FieldHandle fieldHandle;
            if (!executionEnvironment.TryGetFieldFromHandleAndType(runtimeFieldHandle, declaringTypeHandle, out fieldHandle))
            {
                // This may be a field declared on a non-generic type: this api accepts that too so try the other table.
                RuntimeTypeHandle actualDeclaringTypeHandle;
                if (!executionEnvironment.TryGetFieldFromHandle(runtimeFieldHandle, out actualDeclaringTypeHandle, out fieldHandle))
                    throw new ArgumentException(SR.Argument_InvalidHandle);
                if (!actualDeclaringTypeHandle.Equals(declaringTypeHandle))
                    throw new ArgumentException(SR.Format(SR.Argument_ResolveFieldHandle,
                        declaringTypeHandle.GetRuntimeTypeInfoForRuntimeTypeHandle(),
                        actualDeclaringTypeHandle.GetRuntimeTypeInfoForRuntimeTypeHandle()));
            }

            FieldInfo fieldInfo = GetFieldInfo(declaringTypeHandle, fieldHandle);
            return fieldInfo;
        }

        public static EventInfo GetImplicitlyOverriddenBaseClassEvent(EventInfo e)
        {
            return e.GetImplicitlyOverriddenBaseClassMember(EventPolicies.Instance);
        }

        public static MethodInfo GetImplicitlyOverriddenBaseClassMethod(MethodInfo m)
        {
            return m.GetImplicitlyOverriddenBaseClassMember(MethodPolicies.Instance);
        }

        public static PropertyInfo GetImplicitlyOverriddenBaseClassProperty(PropertyInfo p)
        {
            return p.GetImplicitlyOverriddenBaseClassMember(PropertyPolicies.Instance);
        }

        private static RuntimeFieldInfo GetFieldInfo(RuntimeTypeHandle declaringTypeHandle, FieldHandle fieldHandle)
        {
            RuntimeTypeInfo contextTypeInfo = declaringTypeHandle.GetRuntimeTypeInfoForRuntimeTypeHandle();
            NativeFormatRuntimeNamedTypeInfo definingTypeInfo = contextTypeInfo.AnchoringTypeDefinitionForDeclaredMembers.CastToNativeFormatRuntimeNamedTypeInfo();

            // RuntimeFieldHandles always yield FieldInfo's whose ReflectedType equals the DeclaringType.
            RuntimeTypeInfo reflectedType = contextTypeInfo;
            return NativeFormatRuntimeFieldInfo.GetRuntimeFieldInfo(fieldHandle, definingTypeInfo, contextTypeInfo, reflectedType);
        }

        [DebuggerHidden]
        [DebuggerStepThrough]
        public static object ActivatorCreateInstance(
            [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicConstructors | DynamicallyAccessedMemberTypes.NonPublicConstructors)]
            Type type, bool nonPublic)
        {
            return ActivatorImplementation.CreateInstance(type, nonPublic);
        }

        [DebuggerHidden]
        [DebuggerStepThrough]
        public static object ActivatorCreateInstance(
            [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicConstructors | DynamicallyAccessedMemberTypes.NonPublicConstructors)]
            Type type, BindingFlags bindingAttr, Binder? binder, object?[]? args, CultureInfo? culture, object?[]? activationAttributes)
        {
            return ActivatorImplementation.CreateInstance(type, bindingAttr, binder, args, culture, activationAttributes);
        }

        // V2 api: Creates open or closed delegates to static or instance methods - relaxed signature checking allowed.
        public static Delegate CreateDelegate(Type type, object firstArgument, MethodInfo method, bool throwOnBindFailure)
        {
            return CreateDelegateWorker(type, firstArgument, method, throwOnBindFailure, allowClosed: true);
        }

        // V1 api: Creates open delegates to static or instance methods - relaxed signature checking allowed.
        public static Delegate CreateDelegate(Type type, MethodInfo method, bool throwOnBindFailure)
        {
            // This API existed in v1/v1.1 and only expected to create open
            // instance delegates, so we forbid closed delegates for backward compatibility.
            // But we'll allow relaxed signature checking and open static delegates because
            // there's no ambiguity there (the caller would have to explicitly
            // pass us a static method or a method with a non-exact signature
            // and the only change in behavior from v1.1 there is that we won't
            // fail the call).
            return CreateDelegateWorker(type, null, method, throwOnBindFailure, allowClosed: false);
        }

        private static Delegate CreateDelegateWorker(Type type, object firstArgument, MethodInfo method, bool throwOnBindFailure, bool allowClosed)
        {
            ArgumentNullException.ThrowIfNull(type);
            ArgumentNullException.ThrowIfNull(method);

            if (!(type is RuntimeType runtimeDelegateType))
                throw new ArgumentException(SR.Argument_MustBeRuntimeType, nameof(type));

            if (!(method is RuntimeMethodInfo runtimeMethodInfo))
                throw new ArgumentException(SR.Argument_MustBeRuntimeMethodInfo, nameof(method));

            RuntimeTypeInfo runtimeDelegateTypeInfo = runtimeDelegateType.GetRuntimeTypeInfo();

            if (!runtimeDelegateTypeInfo.IsDelegate)
                throw new ArgumentException(SR.Arg_MustBeDelegate, nameof(type));

            Delegate result = runtimeMethodInfo.CreateDelegateNoThrowOnBindFailure(runtimeDelegateTypeInfo, firstArgument, allowClosed);
            if (result == null)
            {
                if (throwOnBindFailure)
                    throw new ArgumentException(SR.Arg_DlgtTargMeth);
                return null;
            }
            return result;
        }

        // V1 api: Creates closed delegates to instance methods only, relaxed signature checking disallowed.
        [RequiresUnreferencedCode("The target method might be removed")]
        public static Delegate CreateDelegate(Type type, object target, string method, bool ignoreCase, bool throwOnBindFailure)
        {
            ArgumentNullException.ThrowIfNull(type);
            ArgumentNullException.ThrowIfNull(target);
            ArgumentNullException.ThrowIfNull(method);

            if (!(type is RuntimeType runtimeDelegateType))
                throw new ArgumentException(SR.Argument_MustBeRuntimeType, nameof(type));

            RuntimeTypeInfo runtimeDelegateTypeInfo = runtimeDelegateType.GetRuntimeTypeInfo();
            if (!runtimeDelegateTypeInfo.IsDelegate)
                throw new ArgumentException(SR.Arg_MustBeDelegate);

            RuntimeTypeInfo runtimeContainingType = target.GetType().ToRuntimeTypeInfo();
            RuntimeMethodInfo runtimeMethodInfo = LookupMethodForCreateDelegate(runtimeDelegateTypeInfo, runtimeContainingType, method, isStatic: false, ignoreCase: ignoreCase);
            if (runtimeMethodInfo == null)
            {
                if (throwOnBindFailure)
                    throw new ArgumentException(SR.Arg_DlgtTargMeth);
                return null;
            }
            return runtimeMethodInfo.CreateDelegateWithoutSignatureValidation(type, target, isStatic: false, isOpen: false);
        }

        // V1 api: Creates open delegates to static methods only, relaxed signature checking disallowed.
        public static Delegate CreateDelegate(Type type, [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.AllMethods)] Type target, string method, bool ignoreCase, bool throwOnBindFailure)
        {
            ArgumentNullException.ThrowIfNull(type);
            ArgumentNullException.ThrowIfNull(target);
            if (target.ContainsGenericParameters)
                throw new ArgumentException(SR.Arg_UnboundGenParam, nameof(target));
            ArgumentNullException.ThrowIfNull(method);

            if (!(type is RuntimeType runtimeDelegateType))
                throw new ArgumentException(SR.Argument_MustBeRuntimeType, nameof(type));

            if (!(target is RuntimeType runtimeContainingType))
                throw new ArgumentException(SR.Argument_MustBeRuntimeType, nameof(target));

            RuntimeTypeInfo runtimeDelegateTypeInfo = runtimeDelegateType.GetRuntimeTypeInfo();

            if (!runtimeDelegateTypeInfo.IsDelegate)
                throw new ArgumentException(SR.Arg_MustBeDelegate);

            RuntimeMethodInfo runtimeMethodInfo = LookupMethodForCreateDelegate(runtimeDelegateTypeInfo, runtimeContainingType.GetRuntimeTypeInfo(), method, isStatic: true, ignoreCase: ignoreCase);
            if (runtimeMethodInfo == null)
            {
                if (throwOnBindFailure)
                    throw new ArgumentException(SR.Arg_DlgtTargMeth);
                return null;
            }
            return runtimeMethodInfo.CreateDelegateWithoutSignatureValidation(type, target: null, isStatic: true, isOpen: true);
        }

        //
        // Helper for the V1/V1.1 Delegate.CreateDelegate() api. These apis take method names rather than MethodInfo and only expect to create open static delegates
        // or closed instance delegates. For backward compatibility, they don't allow relaxed signature matching (which could make the choice of target method ambiguous.)
        //
        [UnconditionalSuppressMessage("ReflectionAnalysis", "IL2075:UnrecognizedReflectionPattern",
            Justification = "Analysis does not track annotations for RuntimeTypeInfo")]
        private static RuntimeMethodInfo LookupMethodForCreateDelegate(RuntimeTypeInfo runtimeDelegateType, RuntimeTypeInfo containingType, string method, bool isStatic, bool ignoreCase)
        {
            Debug.Assert(runtimeDelegateType.IsDelegate);

            BindingFlags bindingFlags = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.ExactBinding;
            if (isStatic)
            {
                bindingFlags |= BindingFlags.Static;
            }
            else
            {
                bindingFlags |= BindingFlags.Instance | BindingFlags.DeclaredOnly;
            }
            if (ignoreCase)
            {
                bindingFlags |= BindingFlags.IgnoreCase;
            }
            RuntimeMethodInfo invokeMethod = runtimeDelegateType.GetInvokeMethod();
            ReadOnlySpan<ParameterInfo> parameters = invokeMethod.GetParametersAsSpan();
            int numParameters = parameters.Length;
            Type[] parameterTypes = new Type[numParameters];
            for (int i = 0; i < numParameters; i++)
            {
                parameterTypes[i] = parameters[i].ParameterType;
            }

            Type? type = containingType.ToType();
            while (type != null)
            {
                MethodInfo? methodInfo = type.GetMethod(method, 0, bindingFlags, parameterTypes);
                if (methodInfo != null && methodInfo.ReturnType.Equals(invokeMethod.ReturnType))
                    return (RuntimeMethodInfo)methodInfo; // This cast is safe since we already verified that containingType is runtime implemented.

                type = type.BaseType;
            }
            return null;
        }

        public static IntPtr GetFunctionPointer(RuntimeMethodHandle runtimeMethodHandle, RuntimeTypeHandle declaringTypeHandle)
        {
            MethodBase method = GetMethodFromHandle(runtimeMethodHandle, declaringTypeHandle);

            switch (method)
            {
                case RuntimeMethodInfo methodInfo:
                    return methodInfo.LdFtnResult;
                case RuntimeConstructorInfo constructorInfo:
                    return constructorInfo.LdFtnResult;
                default:
                    Debug.Fail("RuntimeMethodHandle should only return a methodbase implemented by the runtime.");
                    throw new NotSupportedException();
            }
        }

        public static void MakeTypedReference(object target, FieldInfo[] flds, out Type type, out int offset)
        {
            ArgumentNullException.ThrowIfNull(target);
            ArgumentNullException.ThrowIfNull(flds);
            if (flds.Length == 0)
                throw new ArgumentException(SR.Arg_ArrayZeroError, nameof(flds));

            offset = 0;
            Type targetType = target.GetType();
            for (int i = 0; i < flds.Length; i++)
            {
                if (!(flds[i] is RuntimeFieldInfo field))
                    throw new ArgumentException(SR.Argument_MustBeRuntimeFieldInfo);
                if (field.IsStatic)
                    throw new ArgumentException(SR.Argument_TypedReferenceInvalidField);

                // For proper handling of Nullable<T> don't change to something like 'IsAssignableFrom'
                // Currently we can't make a TypedReference to fields of Nullable<T>, which is fine.
                Type declaringType = field.DeclaringType;
                if (targetType != declaringType && !targetType.IsSubclassOf(declaringType))
                    throw new MissingMemberException(SR.MissingMemberTypeRef); // MissingMemberException is a strange exception to throw, but it is the compatible exception.

                Type fieldType = field.FieldType;
                if (i < (flds.Length - 1) && !fieldType.IsValueType)
                    throw new MissingMemberException(SR.MissingMemberNestErr); // MissingMemberException is a strange exception to throw, but it is the compatible exception.

                targetType = fieldType;
                offset = checked(offset + field.Offset);
            }

            type = targetType;
        }

        public static Assembly[] GetLoadedAssemblies() => RuntimeAssemblyInfo.GetLoadedAssemblies();

        public static EnumInfo GetEnumInfo(Type type, Func<Type, string[], object[], bool, EnumInfo> create)
        {
            RuntimeTypeInfo runtimeType = type.ToRuntimeTypeInfo();

            var info = runtimeType.GenericCache as EnumInfo;
            if (info != null)
                return info;

            ReflectionCoreExecution.ExecutionEnvironment.GetEnumInfo(runtimeType.TypeHandle, out string[] unsortedNames, out object[] unsortedValues, out bool isFlags);

            // Call into IntrospectiveSort directly to avoid the Comparer<T>.Default codepath.
            // That codepath would bring functionality to compare everything that was ever allocated in the program.
            ArraySortHelper<object, string>.IntrospectiveSort(unsortedValues, unsortedNames, EnumUnderlyingTypeComparer.Instance);

            info = create(RuntimeAugments.GetEnumUnderlyingType(type.TypeHandle), unsortedNames, unsortedValues, isFlags);

            runtimeType.GenericCache = info;
            return info;
        }

        private sealed class EnumUnderlyingTypeComparer : IComparer<object>
        {
            public static readonly EnumUnderlyingTypeComparer Instance = new EnumUnderlyingTypeComparer();

            public int Compare(object? x, object? y)
            {
                Debug.Assert(x is byte or ushort or uint or ulong);
                return x switch
                {
                    byte b => b.CompareTo((byte)y!),
                    ushort us => us.CompareTo((ushort)y!),
                    uint ui => ui.CompareTo((uint)y!),
                    _ => ((ulong)x!).CompareTo((ulong)y!),
                };
            }
        }

        public static DynamicInvokeInfo GetDelegateDynamicInvokeInfo(Type type)
        {
            RuntimeTypeInfo runtimeType = type.ToRuntimeTypeInfo();

            DynamicInvokeInfo? info = runtimeType.GenericCache as DynamicInvokeInfo;
            if (info != null)
                return info;

            RuntimeMethodInfo invokeMethod = runtimeType.GetInvokeMethod();

            MethodBaseInvoker methodInvoker = invokeMethod.MethodInvoker;
            IntPtr invokeThunk = ReflectionCoreExecution.ExecutionEnvironment.GetDynamicInvokeThunk(methodInvoker);

            info = new DynamicInvokeInfo(invokeMethod, invokeThunk);
            runtimeType.GenericCache = info;
            return info;
        }

        public static MethodInfo GetDelegateMethod(Delegate del)
        {
            return ReflectionCoreExecution.ExecutionEnvironment.GetDelegateMethod(del);
        }

        public static MethodBase GetMethodBaseFromStartAddressIfAvailable(IntPtr methodStartAddress)
        {
            return ReflectionCoreExecution.ExecutionEnvironment.GetMethodBaseFromStartAddressIfAvailable(methodStartAddress);
        }

        public static Assembly GetAssemblyForHandle(RuntimeTypeHandle typeHandle)
        {
            return Type.GetTypeFromHandle(typeHandle).Assembly;
        }

        public static void RunClassConstructor(RuntimeTypeHandle typeHandle)
        {
            IntPtr pStaticClassConstructionContext = ReflectionCoreExecution.ExecutionEnvironment.GetStaticClassConstructionContext(typeHandle);
            if (pStaticClassConstructionContext != IntPtr.Zero)
            {
                RuntimeAugments.EnsureClassConstructorRun(pStaticClassConstructionContext);
            }
        }
    }
}
