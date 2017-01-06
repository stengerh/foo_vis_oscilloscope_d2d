# Basic Template fo foobar2000 component

## Debugging

The created component can be easily debugged in a portable installation of foobar2000.
The component project is configured to expect such an installation in the `portable`
directory in the solution directory. The expected directory layout is like this:

* $(SolutionDir)
  * `foo_basic_template`
  * `foo_basic_template.sln`
  * `portable`
    * `foobar2000.exe`
