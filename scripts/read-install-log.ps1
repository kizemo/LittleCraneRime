$path = 'C:\Users\Duanyi\AppData\Local\Temp\rime.weasel\install.log'
$text = [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::Unicode)
$text -split "`r?`n" | Where-Object { $_.Trim() } | Select-Object -Last 50
