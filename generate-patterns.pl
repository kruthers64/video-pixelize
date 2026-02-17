#!/usr/bin/perl

use strict;
use warnings;


# config
my $HEADER = "video-pixels-patterns.h";
my $PATTERN_DIR = "patterns";

# data
my $PATTERNS = [
    'square-pinwheel',
    'staggered-2x3',
];


main();
exit();

sub main {
    foreach my $pattern (@$PATTERNS) {
        my $xpm = load_xpm($pattern);
        my $xpm_data = parse_xpm($pattern, $xpm);
        my ($dims, $palette, $pixels) = @$xpm_data;
        my $blarg = xpm_to_numeric($pattern, $palette, $pixels);


#        print("\n$pattern:\n");
#        print("palette has " . $dims->[2] . " colors\n");
#        print(join("\n", @$palette));
#        print("\n");
        
    }
}


sub load_xpm {
    my ($pattern) = @_;

    my $xpm = [];
    my $filename = "$PATTERN_DIR/$pattern.xpm";
    open(my $fh, "<", $filename) or error("File '$filename' not found");
    while (my $line = readline($fh)) {
        push(@$xpm, $line);
    }

    return $xpm;
}


sub parse_xpm {
    my ($pattern, $xpm) = @_;

    my ($dims, $palette, $pixels) = ([], [], []);
    my $state = 0;
    my ($pl, $pc) = (0, 0);
    foreach my $line (@$xpm) {
        $line =~ m/^"(.*)",$/;
        next if (not $1);
        my $match = $1;
        if ($state == 0) {
            $match =~ m/(\d+) +(\d+) +(\d+) +(\d+)/;
            $dims = [$1, $2, $3, $4];
            error("Large palette in pattern '$pattern' not supported yet") if ($dims->[3] != 1);
            $pl = $dims->[2];
            $state = 1;
        } elsif ($state == 1) {
            push(@$palette, $match);
            $pc++;
            if ($pc == $pl) {
                $state = 2;
            }
        } elsif ($state == 2) {
            push(@$pixels, $match);
        }
    }

    return [ $dims, $palette, $pixels ];
}


sub xpm_to_numeric {
    my ($pattern, $xpal, $xpix) = @_;

    my $pal = process_palette($pattern, $xpal);
    my $vixmap = [];
    foreach my $line (@$xpix) {
        
    }
}


sub process_palette {
    my ($pattern, $xpal) = @_;

    my $pal = {
        'white' => undef,
        'black' => undef,
        'red'   => undef,
        'green' => undef,
        'blue'  => undef,
    };

    # first match any named colors (we only accept a very short list) plus white/black hexcodes
    my $hex = {};
    foreach my $line (@$xpal) {
        $line =~ m/^(.)\w+c\w+(.*)$/;
        my ($pixel, $color) = ($1, $2);
        if ($color eq 'white' or $color eq '#FFFFFF' or $color eq '#ffffff') {
            $pal->{'white'} = $pixel;
        } elsif ($color eq 'black' or $color eq '#000000') {
            $pal->{'black'} = $pixel;
        } elsif ($color eq 'red') {
            $pal->{'red'} = $pixel;
        } elsif ($color eq 'green') {
            $pal->{'green'} = $pixel;
        } elsif ($color eq 'blue') {
            $pal->{'blue'} = $pixel;
        } elsif ($color =~ m/^#/) {
            $color = substr($color, 1);
            $hex->{$color} = $pixel;
        } else {
            error("Unrecognized color '$color' in pattern '$pattern'");
        }
    }

    # assign any hex codes to RGB using a simple heuristic
    if (not defined $pal->{'red'}) {
        error("Not enough colors in palette for pattern '$pattern'") if not keys(%$hex);
        my @sorted = sort { $b cmp $a } keys(%$hex);
        my $red = $sorted[0];
        $pal->{'red'} = $hex->{$red};
        delete $hex->{$red};
    }
    if (not defined $pal->{'green'}) {
        error("Not enough colors in palette for pattern '$pattern'") if not keys(%$hex);
        my @sorted = sort { substr($b, 2, 2) cmp substr($a, 2, 2) } keys(%$hex);
        my $green = $sorted[0];
        $pal->{'green'} = $hex->{$green};
        delete $hex->{$green};
    }
    if (not defined $pal->{'blue'}) {
        error("Not enough colors in palette for pattern '$pattern'") if not keys(%$hex);
        my $blue = (keys(%$hex))[0];
        $pal->{'blue'} = $hex->{$blue};
        delete $hex->{$blue};
    }
    error("Too many colors in palette for pattern '$pattern'") if keys(%$hex);

    return $pal;
}


sub error {
    my ($message) = @_;

    my $red = "\e[01;31m";
    my $normal = "\e[00m";
    die "\n${red}ERROR: ${message}${normal}\n";
}
