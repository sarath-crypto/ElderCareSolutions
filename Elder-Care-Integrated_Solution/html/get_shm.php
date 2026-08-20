<?php
$semaphore_key = 98765; 

$shm_key = 0x123456;
$shm_size = 256*1024;
$shm_id = @shmop_open($shm_key, "a", 0, 0);
if (!$shm_id) {
	die("Error: Could not attach to shared memory segment. Ensure C++ app is running.\n");
}

$sem_id = sem_get($semaphore_key, 1, 0666); 
if (!$sem_id) {
	die("PHP: Could not access the shared semaphore.\n");
}else if (sem_acquire($sem_id)) {
	$raw_data = shmop_read($shm_id, 0, $shm_size);
	$size = unpack('V',substr($raw_data, 0, 4))[1];
	$img = substr($raw_data,4,$size);
	$img = rtrim($img, "\0");
	header("Content-Type: image/jpeg");
	header("Content-Length: " . strlen($img));
	echo $img;
	sem_release($sem_id);
	shmop_close($shm_id);
} 

?>


