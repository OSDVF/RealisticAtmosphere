Get-ChildItem -Recurse -filter "*.vert" | foreach{
   $name = $_.FullName | Resolve-Path -Relative
   .\bgfx\.build\win32_vs2017\bin\shadercRelease -f $name -o shaders/glsl/$_.bin --raw --type v --platform windows --profile 440
 }

Get-ChildItem -Recurse -filter "*.frag" | foreach{
   $name = $_.FullName | Resolve-Path -Relative
   .\bgfx\.build\win32_vs2017\bin\shadercRelease -f $name -o shaders/glsl/$_.bin --raw --type f --platform windows --profile 440
}

Get-ChildItem -Recurse -filter "*.comp" | foreach{
   $name = $_.FullName | Resolve-Path -Relative
   .\bgfx\.build\win32_vs2017\bin\shadercRelease -f $name -o shaders/glsl/$_.bin --raw --type c --platform windows --profile 440
}