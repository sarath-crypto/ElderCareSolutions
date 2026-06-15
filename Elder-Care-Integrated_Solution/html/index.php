<?php 
	session_start();
	unset($_SESSION['auth']);
?>

<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">

<style>
body {font-family: Arial, Helvetica, sans-serif;}
form {border: 3px solid #00f1f1;}

input[type=text], input[type=password] {
  width: 20%;
  padding: 12px 20px;
  margin: 8px 0;
  display: inline-block;
  border: 1px solid #ccc;
  box-sizing: border-box;
}

button {
  background-color: #285bd9;
  color: white;
  padding: 14px 20px;
  margin: 8px 0;
  border: none;
  cursor: pointer;
  width: 20%;
}

button:hover {
  opacity: 0.8;
}

.imgcontainer {
  text-align: center;
  margin: 24px 0 12px 0;
}

img.avatar {
  width: 10%;
  border-radius: 50%;
}

.container {
  padding: 16px;
}

span.psw {
  float: right;
  padding-top: 16px;
}

</style>

</head>
<body>

<h2 style="color: red;">This is a private web site, any unautherized attempt to access will be prosecuted to the maximum permitted by the law.</h2>

<form action="main.php" method="post">
  <div class="imgcontainer">
    <img src="login.png" alt="Avatar" class="avatar">
  </div>
  <div class="container" align="middle">
    <input  type="password" placeholder="Enter Password" name="pass_word" required>
    <button type="submit">Login</button>
  </div>
</form>

</body>
</html>

