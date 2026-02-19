#!/usr/bin/perl

use strict;
use warnings;


# config
my $HEADER = "video-pixels-patterns.h";
my $PATTERN_DIR = "patterns";

use Data::Dumper;

main();
exit();

sub main {
    open(my $hfh, ">", $HEADER) or error("Cannot open '$HEADER' for writing");
    write_patterns_preamble($hfh);

    my @files = glob "$PATTERN_DIR/*.xpm";
    my $patterns = [];
    foreach my $filename (@files) {
        my $pattern = (split(m#[/\.]#, $filename))[1];
        push(@$patterns, $pattern);
        my $xpm = load_xpm($filename);

        my ($xpm_dims, $palette, $pixels) = parse_xpm($pattern, $xpm);
        my ($vixmap, $colmap, $dims) = get_pattern_data($pattern, $xpm_dims, $palette, $pixels);

        write_pattern_data($hfh, $pattern, $vixmap, $colmap, $dims);
    }
    write_patterns_array($hfh, $patterns);

    close($hfh);
}


sub load_xpm {
    my ($filename) = @_;

    my $xpm = [];
    open(my $fh, "<", $filename) or error("File '$filename' not found");
    while (my $line = readline($fh)) {
        push(@$xpm, $line);
    }

    return $xpm;
}


sub parse_xpm {
    my ($pattern, $xpm) = @_;

    my ($xpm_dims, $palette, $pixels) = ([], [], []);
    my $state = 0;
    my ($plen, $pcnt) = (0, 0);
    foreach my $line (@$xpm) {
        if ($line =~ m/^"(.*)"/) {
            my $match = $1;
            if ($state == 0) {
                $match =~ m/(\d+) (\d+) (\d+) (\d+)/;
                $xpm_dims = [$1, $2, $3, $4];
                error("Large palette in pattern '$pattern' not supported yet") if ($xpm_dims->[3] != 1);
                $plen = $xpm_dims->[2];
                $state = 1;
            } elsif ($state == 1) {
                push(@$palette, $match);
                $pcnt++;
                if ($pcnt == $plen) {
                    $state = 2;
                }
            } elsif ($state == 2) {
                push(@$pixels, $match);
            }
        }
    }

    return $xpm_dims, $palette, $pixels;
}


sub get_pattern_data {
    my ($pattern, $xpm_dims, $xpal, $xpix) = @_;

    my $pal = process_palette($pattern, $xpal);

    # build vixmap
    my $vixmap = [];
    foreach my $line (@$xpix) {
        foreach my $pixel (split(//, $line)) {
            push(@$vixmap, $pal->{'numbers'}->{$pixel});
        }
    }

    # build colmap
    my $colors = {
        'black' => 0,
        'red'   => 1,
        'green' => 2,
        'blue'  => 3,
    };
    my $colmap = [0];   # always include black
    my $vixn = 1;
    foreach my $pixel (keys(%{$pal->{'colors'}})) {
        my $n = $pal->{'numbers'}->{$pixel};
        my $c = $pal->{'colors'}->{$pixel};
        if ($n > -1) {  # don't include white
            $colmap->[$n] = $colors->{$c};
        }
        # "number of vixels" is highest pixel value plus one, to include black
        if ($n >= $vixn) {
            $vixn = $n + 1;
        }
    }

    # get dimensions
    my ($vw, $vh) = ($xpm_dims->[0], $xpm_dims->[1]);
    my ($gx, $gy, $gw, $gh) = grid_dimensions($vixmap, $vw, $vh);
    my $dims = {
        'gx'    => $gx,
        'gy'    => $gy,
        'gw'    => $gw,
        'gh'    => $gh,
        'vw'    => $vw,
        'vh'    => $vh,
        'vixn'  => $vixn,
    };

    return $vixmap, $colmap, $dims;
}


# convert palette to hash of "pixel" => "color" and hash of "pixel" => "vixel"
sub process_palette {
    my ($pattern, $xpal) = @_;

    my $pal = {
        'colors' => {},
        'numbers' => {},
    };

    my $hex_codes = {};
    my $n = 1;  # vixel number
    foreach my $line (@$xpal) {
        if ($line =~ m/^(.)\s+c\s+(.*)$/) {
            my ($pixel, $color) = ($1, $2);

            my ($hex, $r, $g, $b);
            if ($color =~ m/^#/) {
                $hex = 1;
                ($r, $g, $b) = (lc(substr($color, 1, 2)), lc(substr($color, 3, 2)), lc(substr($color, 5, 2)));
            }

            if ($color eq 'white' or $color eq '#FFFFFF' or $color eq '#ffffff') {
                $pal->{'colors'}->{$pixel} = 'white';
                $pal->{'numbers'}->{$pixel} = -1;
            } elsif ($color eq 'black' or $color eq '#000000') {
                $pal->{'colors'}->{$pixel} = 'black';
                $pal->{'numbers'}->{$pixel} = 0;
            } elsif ($color eq 'red' or ($hex and ($r gt $g and $r gt $b))) {
                $pal->{'colors'}->{$pixel} = 'red';
                $pal->{'numbers'}->{$pixel} = $n++;
            } elsif ($color eq 'green' or ($hex and ($g gt $r and $g gt $b))) {
                $pal->{'colors'}->{$pixel} = 'green';
                $pal->{'numbers'}->{$pixel} = $n++;
            } elsif ($color eq 'blue' or ($hex and ($b gt $r and $b gt $g))) {
                $pal->{'colors'}->{$pixel} = 'blue';
                $pal->{'numbers'}->{$pixel} = $n++;
            } else {
                error("Unrecognized or ambiguous color '$color' in pattern '$pattern'");
            }
        }
    }

    return $pal;
}

sub grid_dimensions {
    my ($vixmap, $vw, $vh) = @_;

    # adapting algorithm from:
    # The Maximal Rectangle Problem from Dr. Dobb's journal By David Vandevoorde, April 01, 1998
    # https://web.archive.org/web/20150921112543/http://www.drdobbs.com/database/the-maximal-rectangle-problem/184410529

    my $c = [(0) x ($vh + 1)];
    my $best_ll = [0, 0];
    my $best_ur = [-1, -1];
    my $best_area = 0;
    my $stack = [];

    my $lookup = sub {
        my ($x, $y) = @_;
        my $idx = $y * $vw + $x;
        return $vixmap->[$idx] != -1;
    };

    my $update_cache = sub {
        my ($x) = @_;
        for (my $y = 0 ; $y < $vh ; $y++) {
            if ($lookup->($x, $y)) {
                $c->[$y]++;
            } else {
                $c->[$y] = 0;
            }
        }
    };

    for (my $x = $vw-1 ; $x >= 0 ; $x--) {
        $update_cache->($x);
        my $width = 0;
        for (my $y = 0 ; $y < $vh ; $y++) {
            if ($c->[$y] > $width) {
                push(@$stack, [$y, $width]);
                $width = $c->[$y];
            }
            if ($c->[$y] < $width) {
                my ($y0, $w0);
                do {
                    ($y0, $w0) = @{pop(@$stack)};
                    my $area = $width * ($y - $y0);
                    if ($area > $best_area) {
                        $best_area = $area;
                        $best_ll = [$x, $y0];
                        $best_ur = [$x + $width - 1, $y - 1];
                    }
                    $width = $w0;
                } while ($c->[$y] < $width);
                $width = $c->[$y];
                if ($width != 0) {
                    push(@$stack, [$y0, $width]);
                }
            }
        }
    }

    return $best_ll->[0], $best_ll->[1], $best_ur->[0] - $best_ll->[0] + 1, $best_ur->[1] - $best_ll->[1] + 1; 

}


sub write_patterns_array {
    my ($hfh, $patterns) = @_;

    my $n = scalar(@$patterns);
    print($hfh "static Pattern *patterns[$n] = {");
    foreach my $pattern (@$patterns) {
        my $safe_name = safe_pattern_name($pattern);
        print($hfh " &${safe_name},");
    }
    print($hfh " };\n");
}


sub write_patterns_preamble {
    my ($hfh) = @_;

    print($hfh "/*\n");
    print($hfh " * Do not edit this file.  It is automatically generated based on xpm image\n");
    print($hfh " * files in the \"patterns\" subdirectory.  Any changes here may be overwritten.\n");
    print($hfh "*/\n\n\n");
}


sub write_pattern_data {
    my ($hfh, $pattern, $vixmap, $colmap, $dims) = @_;

    my $safe_name = safe_pattern_name($pattern);
    my ($vw, $vh) = ($dims->{'vw'}, $dims->{'vh'});

    print($hfh "gint ${safe_name}_vixmap[${vw}*${vh}] = {\n");
    for (my $idx = 0 ; $idx < scalar(@$vixmap) ; $idx++) {
        if ($idx % $vw == 0) {
            print($hfh "   ");
        }
        printf($hfh " %3d,", $vixmap->[$idx]);
        if (($idx + 1) % $vw == 0) {
            print($hfh "\n");
        }
    };
    print($hfh "};\n");

    print($hfh "gint ${safe_name}_colmap[" . $dims->{'vixn'} . "] = {\n");
    for (my $idx = 0 ; $idx < scalar(@$colmap) ; $idx++) {
        if ($idx % 10 == 0) {
            print($hfh "   ");
        }
        printf($hfh " %3d,", $colmap->[$idx]);
        if (($idx + 1) % 10 == 0 or $idx == scalar(@$colmap) - 1) {
            print($hfh "\n");
        }
    }
    print($hfh "};\n");
    print($hfh "Pattern ${safe_name} = {\n");
    print($hfh "    ");
    print($hfh $dims->{'gx'} . ", ");
    print($hfh $dims->{'gy'} . ", ");
    print($hfh $dims->{'gw'} . ", ");
    print($hfh $dims->{'gh'} . ", ");
    print($hfh $dims->{'vixn'} . ", ");
    print($hfh "${safe_name}_vixmap, ");
    print($hfh $dims->{'vw'} . ", ");
    print($hfh $dims->{'vh'} . ", ");
    print($hfh "${safe_name}_colmap\n");
    print($hfh "};\n");
    print($hfh "\n");
}


sub write_patterns_array {
    my ($hfh, $patterns) = @_;

    my $n = scalar(@$patterns);
    print($hfh "static Pattern *patterns[$n] = {");
    foreach my $pattern (@$patterns) {
        my $safe_name = safe_pattern_name($pattern);
        print($hfh " &${safe_name},");
    }
    print($hfh " };\n");
}


sub safe_pattern_name {
    my ($pattern) = @_;
    $pattern =~ s/[^a-z0-9]/_/;
    return $pattern;
}



sub error {
    my ($message) = @_;

    my $red = "\e[01;31m";
    my $normal = "\e[00m";
    die "\n${red}ERROR: ${message}${normal}\n";
}
