echo "fp_1"
bunzip2 -kc ../traces/fp_1.bz2 | ./predictor --tournament:9:10:10 
echo "fp_2"
bunzip2 -kc ../traces/fp_2.bz2 | ./predictor --tournament:9:10:10
echo "int_1"
bunzip2 -kc ../traces/int_1.bz2 | ./predictor --tournament:9:10:10
echo "int_2"
bunzip2 -kc ../traces/int_2.bz2 | ./predictor --tournament:9:10:10
echo "mm_1"
bunzip2 -kc ../traces/mm_1.bz2 | ./predictor --tournament:9:10:10
echo "mm_2"
bunzip2 -kc ../traces/mm_2.bz2 | ./predictor --tournament:9:10:10

