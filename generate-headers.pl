#!/usr/bin/perl

use strict;
use warnings;


# config
my $HEADER_PATTERNS = "video-pixelize-patterns.h";
my $HEADER_GEGL_ENUM = "video-pixelize-gegl-enum.h";
my $HEADER_GEGL_ENUM_CORE = "video-pixelize-core-gegl-enum.h";
my $PATTERN_DIR = "patterns";
my $GRID_SUFFIX = "GRID";
my $GEGL_ENUM_PREFIX = "GEGL_VIDEO_PIXELIZE_TYPE";
my $GEGL_ENUM_PREFIX_CORE = "GEGL_VIDEO_PIXELIZE_CORE_TYPE";
my $GEGL_ENUM_TEMPLATE = '
enum_start (gegl_video_pixelize_type)
__ENUM_VALUES__
enum_end (GeglVideoPixelizeType)

property_enum (pattern, _("Pattern"), GeglVideoPixelizeType,
    gegl_video_pixelize_type, __DEFAULT_ENUM_VALUE__)
description (_("Video pixel pattern"))
';
my $GEGL_ENUM_TEMPLATE_CORE = '
enum_start (gegl_video_pixelize_core_type)
__ENUM_VALUES__
enum_end (GeglVideoPixelizeCoreType)

property_enum (pattern, _("Pattern"), GeglVideoPixelizeCoreType,
    gegl_video_pixelize_core_type, __DEFAULT_ENUM_VALUE__)
description (_("Video pixel pattern"))
';


use Data::Dumper;

main();
exit();

sub main {
    my @files = glob "$PATTERN_DIR/*.xpm";
    my $patterns = [];
    my ($pattern_text, $gegl_enum_text, $gegl_enum_text_core) = ("", "", "");
    foreach my $filename (@files) {
        # GRID files augment patterns; don't treat them as regular patterns themselves
        if ($filename =~ m/$GRID_SUFFIX.xpm/) {
            next;
        }
        my $pattern = (split(m#[/\.]#, $filename))[1];
        push(@$patterns, $pattern);
        my $xpm = load_xpm($filename);

        my ($xpm_dims, $palette, $pixels) = parse_xpm($pattern, $xpm);
        my ($vixmap, $colmap, $dims) = get_pattern_data($pattern, $xpm_dims, $palette, $pixels);

        # look for matching GRID file to correct grid dimensions
        my $grid_filename = "$PATTERN_DIR/$pattern-$GRID_SUFFIX.xpm";
        if (-e $grid_filename) {
            my $grid_name = (split(m#[/\.]#, $filename))[1];
            my $grid_xpm = load_xpm($grid_filename);
            my ($grid_xpm_dims, $grid_palette, $grid_pixels) = parse_xpm($grid_name, $grid_xpm);
            my ($grid_vixmap, $grid_colmap, $grid_dims) = get_pattern_data($grid_name, $grid_xpm_dims, $grid_palette, $grid_pixels);
            $dims->{'gw'} = $grid_dims->{'gw'};
            $dims->{'gh'} = $grid_dims->{'gh'};
        }

        $pattern_text        .= generate_pattern_data($pattern, $vixmap, $colmap, $dims);
        $gegl_enum_text      .= generate_gegl_enum_text($pattern, $GEGL_ENUM_PREFIX);
        $gegl_enum_text_core .= generate_gegl_enum_text($pattern, $GEGL_ENUM_PREFIX_CORE);
    }

    # write files
    open(my $pfh, ">", $HEADER_PATTERNS) or error("Cannot open '$HEADER_PATTERNS' for writing");
    open(my $gfh, ">", $HEADER_GEGL_ENUM) or error("Cannot open '$HEADER_GEGL_ENUM' for writing");
    open(my $cfh, ">", $HEADER_GEGL_ENUM_CORE) or error("Cannot open '$HEADER_GEGL_ENUM_CORE' for writing");

    write_preamble($pfh);
    write_preamble($gfh);
    write_preamble($cfh);
    print($pfh $pattern_text);
    write_patterns_array($pfh, $patterns);
    write_gegl_enum_file($gfh, $patterns, $gegl_enum_text,      $GEGL_ENUM_TEMPLATE,      $GEGL_ENUM_PREFIX);
    write_gegl_enum_file($cfh, $patterns, $gegl_enum_text_core, $GEGL_ENUM_TEMPLATE_CORE, $GEGL_ENUM_PREFIX_CORE);

    close($pfh);
    close($gfh);
    close($cfh);
}


# ---- XPM handling ----------------------------------------------------------------------------------


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


# ---- Pattern parsing -------------------------------------------------------------------------------


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
    my $colmap = [0];   # always include black (bg color)
    my $vixn = 1;
    foreach my $pixel (keys(%{$pal->{'colors'}})) {
        my $n = $pal->{'numbers'}->{$pixel};
        my $c = $pal->{'colors'}->{$pixel};
        if ($n > -1) {  # don't include white (unused pixels)
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
        my ($n, $m) = @_;
        my $idx = $m * $vw + $n;
        return $vixmap->[$idx] != -1;
    };

    my $update_cache = sub {
        my ($n) = @_;
        for (my $m = 0 ; $m < $vh ; $m++) {
            if ($lookup->($n, $m)) {
                $c->[$m]++;
            } else {
                $c->[$m] = 0;
            }
        }
    };

    for (my $n = $vw-1 ; $n >= 0 ; $n--) {
        $update_cache->($n);
        my $open_width = 0;
        for (my $m = 0 ; $m <= $vh ; $m++) {
            if ($c->[$m] > $open_width) {
                push(@$stack, [$m, $open_width]);
                $open_width = $c->[$m];
            }
            if ($c->[$m] < $open_width) {
                my ($y0, $w0);
                do {
                    ($y0, $w0) = @{pop(@$stack)};
                    my $area = $open_width * ($m - $y0);
                    if ($area > $best_area) {
                        $best_area = $area;
                        $best_ll = [$n, $y0];
                        $best_ur = [$n + $open_width - 1, $m - 1];
                    }
                    $open_width = $w0;
                } while ($c->[$m] < $open_width);
                $open_width = $c->[$m];
                if ($open_width != 0) {
                    push(@$stack, [$y0, $w0]);
                }
            }
        }
    }

    my $x = $best_ll->[0];
    my $y = $best_ll->[1];
    my $w = $best_ur->[0] - $best_ll->[0] + 1;
    my $h = $best_ur->[1] - $best_ll->[1] + 1;

    return $x, $y, $w, $h;
}


# ---- Header writing --------------------------------------------------------------------------------


sub write_preamble {
    my ($fh) = @_;

    print($fh "/*\n");
    print($fh " * Do not edit this file; any changes will be overwritten.  It is automatically\n");
    print($fh " * created by generate-headers.pl based on xpm image files in the \"patterns\"\n");
    print($fh " * subdirectory.\n");
    print($fh "*/\n\n");
}


sub generate_pattern_data {
    my ($pattern, $vixmap, $colmap, $dims) = @_;

    my $identifier = get_c_identifier($pattern);
    my ($vw, $vh) = ($dims->{'vw'}, $dims->{'vh'});

    my $text = "";

    $text .= "gint ${identifier}_vixmap[${vw}*${vh}] = {\n";
    for (my $idx = 0 ; $idx < scalar(@$vixmap) ; $idx++) {
        if ($idx % $vw == 0) {
            $text .= "   ";
        }
        $text .= sprintf(" %3d,", $vixmap->[$idx]);
        if (($idx + 1) % $vw == 0) {
            $text .= "\n";
        }
    };
    $text .= "};\n";

    $text .= "gint ${identifier}_colmap[" . $dims->{'vixn'} . "] = {\n";
    for (my $idx = 0 ; $idx < scalar(@$colmap) ; $idx++) {
        if ($idx % 10 == 0) {
            $text .= "   ";
        }
        $text .= sprintf(" %3d,", $colmap->[$idx]);
        if (($idx + 1) % 10 == 0 or $idx == scalar(@$colmap) - 1) {
            $text .= "\n";
        }
    }
    $text .= "};\n";
    $text .= "Pattern ${identifier} = {\n";
    $text .= "    ";
    $text .= $dims->{'gx'} . ", ";
    $text .= $dims->{'gy'} . ", ";
    $text .= $dims->{'gw'} . ", ";
    $text .= $dims->{'gh'} . ", ";
    $text .= $dims->{'vixn'} . ", ";
    $text .= "${identifier}_vixmap, ";
    $text .= $dims->{'vw'} . ", ";
    $text .= $dims->{'vh'} . ", ";
    $text .= "${identifier}_colmap\n";
    $text .= "};\n";
    $text .= "\n";

    return $text;
}


sub write_patterns_array {
    my ($fh, $patterns) = @_;

    my $n = scalar(@$patterns);
    print($fh "static Pattern *patterns[$n] = {");
    foreach my $pattern (@$patterns) {
        my $identifier = get_c_identifier($pattern);
        print($fh " &${identifier},");
    }
    print($fh " };\n");
}


sub generate_gegl_enum_text {
    my ($pattern, $prefix) = @_;
    my $pretty = ucfirst($pattern);
    $pretty =~ s/[_-]/ /g;
    my $enum = "    enum_value (" . get_gegl_property($pattern, $prefix) . ", \"${pattern}\",\n";
    $enum .= "        N_(\"${pretty}\"))\n";
    return $enum;
}


sub write_gegl_enum_file {
    my ($fh, $patterns, $gegl_enum_text, $template, $prefix) = @_;
    my $default_prop = get_gegl_property($patterns->[0], $prefix);
    $gegl_enum_text =~ s/\s+$//;
    $template =~ s/^\s+//;
    $template =~ s/__ENUM_VALUES__/$gegl_enum_text/;
    $template =~ s/__DEFAULT_ENUM_VALUE__/$default_prop/;
    print($fh $template);
}


# ---- Utility functions ------------------------------------------------------------------------------


sub get_c_identifier {
    my ($pattern) = @_;
    $pattern =~ s/[^a-z0-9]/_/;
    return $pattern;
}

sub get_gegl_property {
    my ($pattern, $prefix) = @_;
    return "${prefix}_" . uc(get_c_identifier($pattern));
}

sub error {
    my ($message) = @_;

    my $red = "\e[01;31m";
    my $normal = "\e[00m";
    die "\n${red}ERROR: ${message}${normal}\n";
}
