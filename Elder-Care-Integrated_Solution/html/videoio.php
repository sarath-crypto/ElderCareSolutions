<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Continuous Live Image Feed</title>
    <style>
        body {
            font-family: Arial, sans-serif;
            display: flex;
            flex-direction: column;
            align-items: center;
            justify-center;
            background-color: #000000;
            margin: 0;
            padding: 1px;
        }
        .image-container {
            margin-top: 1px;
            box-shadow: 0 4px 8px rgba(0,0,0,0.1);
            border-radius: 8px;
            overflow: hidden;
            background: #fff;
        }
        img {
            display: block;
            max-width: 100%;
            height: auto;
            max-height: 500px;
        }
        .status {
            margin-top: 10px;
            font-size: 14px;
            color: #666;
        }
    </style>
</head>
<body>
    
    <div class="image-container">
        <!-- The source points initially to the PHP script -->
        <img id="liveImage" src="fetch_image.php" alt="Live Stream Database Image">
    </div>
    
    <div class="status" id="statusUpdate">Last updated: Just now</div>

    <script>
        // Set interval timing in milliseconds (e.g., 2000ms = 2 seconds)
        const FETCH_INTERVAL = 500; 
        const imageElement = document.getElementById('liveImage');
        const statusElement = document.getElementById('statusUpdate');

        function updateImage() {
            // Append a unique timestamp to force the browser to bypass its cache
            const timestamp = new Date().getTime();
            imageElement.src = 'fetch_image.php?t=' + timestamp;
            
            // Optional: Update status text on the UI
            const now = new Date().toLocaleTimeString();
            statusElement.textContent = 'Last updated: ' + now;
        }

        // Continuously run the update function at the specified interval
        setInterval(updateImage, FETCH_INTERVAL);
    </script>

</body>
</html>
