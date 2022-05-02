$FolderName = "shaders/glsl/"
if ( -not(Test-Path $FolderName) )
{
    New-Item $FolderName -ItemType Directory
}

Get-ChildItem -Recurse -filter "*.vert" | foreach{
   $name = $_.FullName | Resolve-Path -Relative
   .\bgfx\.build\win32_vs2017\bin\shadercDebug -f $name -o shaders/glsl/$_.bin --type v --platform windows --profile 440 --define DEBUG | Out-Null
 }

Get-ChildItem -Recurse -filter "*.frag" | foreach{
   $name = $_.FullName | Resolve-Path -Relative
   .\bgfx\.build\win32_vs2017\bin\shadercDebug -f $name -o shaders/glsl/$_.bin --type f --platform windows --profile 440 --define DEBUG | Out-Null
}

Get-ChildItem -Recurse -filter "*.comp" | foreach{
   $name = $_.FullName | Resolve-Path -Relative
   .\bgfx\.build\win32_vs2017\bin\shadercDebug -f $name -o shaders/glsl/$_.bin --type c --platform windows --profile 440 --define DEBUG | Out-Null
}