const char webpage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta http-equiv="refresh" content="3">
<title>Parkeringssystem</title>

<style>
body{
    font-family:Arial;
    text-align:center;
    background:#f0f0f0;
}

h1{
    color:#333;
}

.full{
    color:red;
    font-size:2em;
    font-weight:bold;
}

.warning{
    color:orange;
    font-size:2em;
}

.ledigt{
    color:green;
    font-size:2em;
}

.info{
    color:#666;
}
</style>
</head>

<body>

<h1>Parkeringsplatser</h1>

<p class="info">
Max antal platser: %MAX%
</p>

%STATUS%

</body>
</html>
)rawliteral";