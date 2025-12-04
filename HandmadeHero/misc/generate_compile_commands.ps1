# Generate compile_commands.json for clangd
$projectRoot = Split-Path $MyInvocation.MyCommand.Path -Parent
$projectRoot = Split-Path $projectRoot -Parent

$compileCommands = @(
    @{
        directory = $projectRoot.Replace('\', '/')
        command = "clang++ -g -O0 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-missing-field-initializers -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -fdiagnostics-absolute-paths -target x86_64-pc-windows-msvc -fms-extensions -fms-compatibility -fdelayed-template-parsing -Icode code/handmade.cpp"
        file = "code/handmade.cpp"
    },
    @{
        directory = $projectRoot.Replace('\', '/')
        command = "clang++ -g -O0 -Wall -Wextra -Wno-unused-parameter -Wno-unused-variable -Wno-missing-field-initializers -DHANDMADE_SLOW=1 -DHANDMADE_INTERNAL=1 -fdiagnostics-absolute-paths -target x86_64-pc-windows-msvc -fms-extensions -fms-compatibility -fdelayed-template-parsing -Icode code/win32_handmade.cpp"
        file = "code/win32_handmade.cpp"
    }
)

$json = $compileCommands | ConvertTo-Json -Depth 3
$outputPath = Join-Path $projectRoot "compile_commands.json"
$json | Out-File -FilePath $outputPath -Encoding UTF8

Write-Host "Generated compile_commands.json at: $outputPath"
