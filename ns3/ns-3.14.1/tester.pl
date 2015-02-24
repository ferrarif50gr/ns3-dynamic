#!/usr/bin/perl -w
use strict; 
use warnings;

my @seeds=(1, 2);
my @interarrivals = (0.002, 0.0002);
my @queueSizes = (1, 2);
my @packetSizes = (146, 1446);
#open (simResults, ">FinalResult.txt") || die "File not found\n";
#	#print "Press any key to continue\n"; my $pause = <STDIN>;	

#		#system ($^X,"AppGen.pl 5ss 5");# this is a useful command to run another script with the arguments passed 
		
#my $i= 1;

#my %Tput = ();
#my $scen=$ARGV[0].'ss';
foreach my $seed (@seeds)
{
#		unlink("Tput.dat");
	&replacer ('IntegerValue \(\d+\)', "IntegerValue \($seed\)","/workspace/ns-3/ns-3.14.1/scratch/MyScenario2.cc");
	foreach my $interarrival (@interarrivals)
	{
	        &replacer ('ExponentialVariable\(0\.[0-9]*\)', "ExponentialVariable\($interarrival\)","/workspace/ns-3/ns-3.14.1/scratch/MyScenario2.cc");
		foreach my $queueSize (@queueSizes)
		{
			&replacer ('\"MaxQueueLength\",UintegerValue\([0-9]*', "\"MaxQueueLength\",UintegerValue\($queueSize","/workspace/ns-3/ns-3.14.1/scratch/MyScenario2.cc");
#			foreach my $packetSize (@packetSizes)
#			{
#				&replacer ('\"PacketSize\",RandomVariableValue\(ConstantVariable \(\d+\)', "\"PacketSize\",RandomVariableValue\(ConstantVariable \($packetSize\)");
                		&replacer ('switched-ethernet-My2-[0-9]*-0\.[0-9]*-1500-0.0002-1500-[0-9]-exp', "switched-ethernet-My2-$queueSize-$interarrival-1500-0.0002-1500-$seed-exp", "/workspace/ns-3/ns-3.14.1/scratch/MyScenario2.cc");

				system ( "./waf --run MyScenario2");
#			}
		}
	}
#		if ( $? == -1 )
#		{
#			print "UNABLE TO RUN QUALNET 5.0 FROM PERL: $!\n";
#		}
#		else
#		{
			
#			my $temp= &tputReader();
#			$Tput{$i} = $temp;
#			print "\n ****************************************************************************\n"; 
#			print "			SEED: $seed Batch # $i is done!\n ";
#			print "			Throughput = $temp \n ";
#			print "****************************************************************************\n";
#
#		}

	
#print "@Tput\n";
#	&replacer ('IntegerValue \(\d+\)', "IntegerValue \($seed\)","/workspace/ns-3/ns-3.14.1/scratch/MyScenario2.cc");
#	$i++;
}

#foreach my $i (sort keys %Tput){
#		print simResults "$i\t$Tput{$i}\n"; 
#		print "$i\t$Tput{$i}\n";}
#print simResults "#################################################\n";
#close simResults;



#&CICalculator(\%Tput);
#--------------------------------------------------- SUBROUTINES ---------------------------------------------------


############################
#FUNCTION :: REPLACER 
#PURPOSE :: this function will search the desired file, find the expression and replace it.
############################

sub replacer{

	my $searchStr=$_[0];
	my $replaceStr=$_[1];
	my $fileName=$_[2];
	#print ("Search String:",$searchStr,"\n");
	#print ("Replace String:",$replaceStr,"\n\n");

	open(FILE,"$fileName") || die("Cannot Open File");
	my(@fcont) = <FILE>;
	close FILE;

	open(FOUT,">$fileName") || die("Cannot Open File");
	foreach my $line (@fcont) {
		$line =~ s/$searchStr/$replaceStr/g;
		print FOUT $line;
	}
close FOUT;
}


############################
#FUNCTION :: TPUTREADER 
#PURPOSE :: this function reads the throughput of individual nodes and sum them up to find the total network throughput.
############################

sub tputReader{

my $networkTput=0;
open (LIST, "Tput.dat") || die "File not found\n";
while (<LIST>) {
	my @data = split /\t+/;
	 $networkTput += $data[1];
}
close(LIST);
#print "$networkTput\n";
$networkTput = sprintf("%.2f", $networkTput);
return $networkTput;
}

sub CICalculator{

my %Tput = %{$_[0]};
my $n=scalar keys %Tput;
my $sum=0;
my $Mean=0;
my $Variance=0;

foreach my $i (keys %Tput){
	 $sum += $Tput{$i};
}

$Mean= $sum/$n;

foreach my $i (keys %Tput) {
	$Variance+= ($Tput{$i} - $Mean)**2;
	}
$Variance=$Variance/($n-1);
my $CI = 2.093 * sqrt( $Variance/$n);
$CI = sprintf("%.4f", $CI);
print "the confidence interval is $CI\n mean = $Mean\n";

}
