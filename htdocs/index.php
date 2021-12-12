<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8" />
    <meta http-equiv="X-UA-Compatible" content="IE=edge" />
    <meta name="viewport" content="width=device-width, initial-scale=1.0" />
    <title>Home</title>
    <link rel="stylesheet" href="style.css" />
    <link rel="stylesheet" href="https://pro.fontawesome.com/releases/v5.10.0/css/all.css" integrity="sha384-AYmEC3Yw5cVb3ZcuHtOA93w35dYTsvhLPVnYs9eStHfGJvOvKxVfELGroGkvsg+p" crossorigin="anonymous" />
</head>

<body>
    <nav>
        <div class="navbar">
            <div class="logo">My WebServer</div>
            <div class="primary">
                <a href="index.html" class="menu-item">Home</a>
                <a href="docs.html" class="menu-item">Files</a>
                <a href="index.php" class="menu-item">PHP</a>
                <a href="about.html" class="menu-item">About</a>
            </div>
            <div class="secondary">
                <a target="_blank" href="https://github.com/kesaralive/webserver.git" class="github"><i class="fab fa-github"></i></a>
                <a target="_blank" href="https://hub.docker.com/repository/docker/kesaralive/simple_webserver" class="docker"><i class="fab fa-docker"></i></a>
            </div>
        </div>
    </nav>

    <section>
        <!-- php code segment  -->
        <?php
        echo "PHP is working";
        ?>

        <?php
        for ($i = 0; $i < 5; $i++) {
        ?>
            <div>
                <h1>Heading<?php echo $i; ?></h1>
                <p>Lorem ipsum dolor sit amet.</p>
            </div>
        <?
        }
        ?>
    </section>
</body>

</html>