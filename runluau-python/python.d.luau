export type PythonObject = {
    [string]: any,
}

declare python: {
    import: (moduleName: string) -> PythonObject,
    eval: (code: string) -> any,
    call: (callable: PythonObject, ...any) -> any,
    get_attr: (obj: PythonObject, attr: string) -> any,
    set_attr: (obj: PythonObject, attr: string, value: any) -> boolean,
    to_string: (obj: PythonObject) -> string,
    is_callable: (obj: PythonObject) -> boolean
}