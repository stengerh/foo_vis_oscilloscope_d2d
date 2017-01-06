# Basic template for foobar2000 plugins

## Version information

A foobar2000 plugin can contain version information in two forms:

* as a version resource
* as a foobar2000 component version declaration

The information in the version resource is used by Windows Explorer in the properties sheet of the DLL.
The component version declaration is used by foobar2000 on the components page of the preferences window.

The template defines most of the version information as macros in `version.h`.
This information is used in `version.rc` to generate the version resource and in `version.cpp` to implement the component version declaration.

## Component validation

A foobar2000 plugin can implement a component validation service to check whether it was correctly installed.
A default implementation of this service is provided by the foobar2000 SDK.
This default implementation checks the name of the plugin DLL. If the DLL was renamed the ckeck will fail.

The template uses this default validation.
The macro COMPONENT_FILENAME must be defined in `version.h` or in the project's preprocessor settings.
By default the macro is set to the target filename in the preprocessor settings.

## Debugging

The created plugin can be easily debugged in a portable installation of foobar2000.
The plugin project is configured to expect such an installation in the `portable`
directory in the solution directory. The expected directory layout is like this:

* $(SolutionDir)
  * `foo_basic_template`
  * `foo_basic_template.sln`
  * `portable`
    * `foobar2000.exe`
