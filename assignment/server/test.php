<html>
<body>
<?php
$time1 = microtime(true);
#echo $time1."<br/><br/>";
$val=400000;
for($i=1;$i<=$val;$i++);
$time2 = microtime(true);
#echo $time2."<br/><br/>";
$time=$time2-$time1;
echo "Time taken = $time sec";
?>
</html>
</body>