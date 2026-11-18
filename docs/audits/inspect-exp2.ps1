Add-Type -AssemblyName System.IO.Compression.FileSystem
$materials = $env:CHORDSLOTS_AUDIT_MATERIALS
if (-not $materials) { $materials = Join-Path $PSScriptRoot 'materials' }
$path = Join-Path $materials 'Experiment2/ResulstListeningTest.xlsx'
$zip = [IO.Compression.ZipFile]::OpenRead($path)
try {
    function Read-Xml($entry) {
        $reader = [IO.StreamReader]::new($entry.Open())
        try { [xml]$reader.ReadToEnd() } finally { $reader.Dispose() }
    }
    $strings = (Read-Xml $zip.GetEntry('xl/sharedStrings.xml')).sst.si | ForEach-Object { $_.InnerText }
    $book = Read-Xml $zip.GetEntry('xl/workbook.xml')
    $book.workbook.sheets.sheet | ForEach-Object { "Sheet: $($_.name)" }
    foreach ($entry in $zip.Entries | Where-Object FullName -Match '^xl/worksheets/sheet\d+\.xml$') {
        $sheet = (Read-Xml $entry).worksheet
        $rows = @($sheet.sheetData.row)
        "Entry: $($entry.FullName), range $($sheet.dimension.ref), rows $($rows.Count)"
        ($rows[0].c | ForEach-Object { "$($_.r)=$($strings[[int]$_.v])" }) -join ' | '
        $responses = @($rows | Where-Object {
            $_.r -ne '1' -and $_.c[0].r -match '^A\d+$' -and $_.c[0].v -match '^\d+(\.\d+)?$'
        })
        "Numeric date rows: $($responses.Count)"
        $complete = @($responses | Where-Object {
            @($_.c | Where-Object { $_.r -match '^[B-M]\d+$' -and $_.v -match '^[1-6]$' }).Count -eq 12
        })
        "Complete response rows: $($complete.Count)"
        foreach ($col in [char[]]'BCDEFGHIJKLM') {
            $values = @($responses | ForEach-Object {
                $_.c | Where-Object { $_.r -match "^$col\d+$" -and $_.v -match '^[1-6]$' } | ForEach-Object v
            })
            "$col N=$($values.Count) " + (($values | Group-Object | Sort-Object Name | ForEach-Object { "$($_.Name)=$($_.Count)" }) -join ' ')
        }
    }
} finally { $zip.Dispose() }
