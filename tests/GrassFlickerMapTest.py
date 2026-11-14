import importlib.util
import unittest
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("grass_flicker_map", ROOT / "tools" / "grass-flicker-map.py")
grass_flicker_map = importlib.util.module_from_spec(spec)
assert spec.loader is not None
spec.loader.exec_module(grass_flicker_map)


def series(values):
    """A frame series of one pixel row: values[frame][pixel]."""
    return np.asarray(values, dtype=np.float32)[:, np.newaxis, :]


class FlickerMapTest(unittest.TestCase):
    """What each pixel does frame to frame: a jump back and forth counts,
    a steady drift does not."""

    def test_a_still_pixel_does_not_flicker(self):
        m = grass_flicker_map.flicker_map(series([[10], [10], [10], [10]]))
        self.assertEqual(m.shape, (1, 1))
        self.assertEqual(m[0, 0], 0.0)

    def test_a_steady_drift_cancels(self):
        m = grass_flicker_map.flicker_map(series([[10], [20], [30], [40]]))
        self.assertEqual(m[0, 0], 0.0)

    def test_a_pixel_jumping_back_and_forth_flickers(self):
        # 0, 100, 0, 100: both second differences are 200.
        m = grass_flicker_map.flicker_map(series([[0], [100], [0], [100]]))
        self.assertEqual(m[0, 0], 200.0)

    def test_each_pixel_is_judged_alone(self):
        m = grass_flicker_map.flicker_map(series([[0, 5], [100, 5], [0, 5]]))
        self.assertEqual(m[0, 0], 200.0)
        self.assertEqual(m[0, 1], 0.0)

    def test_fewer_than_three_frames_give_no_map(self):
        self.assertIsNone(grass_flicker_map.flicker_map(series([[0], [100]])))


class ConcentrationTest(unittest.TestCase):
    """Whether the flicker sits in few pixels (edges, crossings) or all over."""

    def test_the_share_of_pixels_above_the_threshold_and_what_they_carry(self):
        m = np.asarray([[0.0, 0.0, 0.0, 40.0]], dtype=np.float32)
        pixels, carried = grass_flicker_map.concentration(m, 10.0)
        self.assertEqual(pixels, 0.25)
        self.assertEqual(carried, 1.0)

    def test_a_still_picture_carries_nothing(self):
        m = np.zeros((2, 2), dtype=np.float32)
        self.assertEqual(grass_flicker_map.concentration(m, 10.0), (0.0, 0.0))

    def test_flicker_below_the_threshold_stays_with_the_rest(self):
        m = np.asarray([[5.0, 5.0, 30.0, 0.0]], dtype=np.float32)
        pixels, carried = grass_flicker_map.concentration(m, 10.0)
        self.assertEqual(pixels, 0.25)
        self.assertEqual(carried, 0.75)


class BoxesTest(unittest.TestCase):
    """The boxes of the picture a run is read by, name:left:top:right:bottom."""

    def test_boxes_are_parsed(self):
        self.assertEqual(grass_flicker_map.parse_boxes("grass:0:1:2:3,lawn:4:5:6:7"),
                         [("grass", 0, 1, 2, 3), ("lawn", 4, 5, 6, 7)])

    def test_a_box_without_all_four_edges_is_refused(self):
        with self.assertRaises(ValueError):
            grass_flicker_map.parse_boxes("grass:0:1:2")

    def test_an_empty_box_is_refused(self):
        with self.assertRaises(ValueError):
            grass_flicker_map.parse_boxes("grass:5:5:5:9")

    def test_a_box_mean_reads_its_pixels_only(self):
        m = np.asarray([[1.0, 3.0], [5.0, 7.0]], dtype=np.float32)
        self.assertEqual(grass_flicker_map.box_mean(m, ("b", 1, 0, 2, 2)), 5.0)

    def test_a_box_outside_the_picture_has_no_mean(self):
        m = np.zeros((2, 2), dtype=np.float32)
        self.assertIsNone(grass_flicker_map.box_mean(m, ("b", 4, 4, 8, 8)))


class HeatTest(unittest.TestCase):
    """The map as a picture: still pixels dark, the full value white."""

    def test_the_heat_is_scaled_to_the_full_value_and_clipped(self):
        m = np.asarray([[0.0, 20.0, 40.0, 400.0]], dtype=np.float32)
        heat = grass_flicker_map.heat(m, 40.0)
        self.assertEqual(heat.dtype, np.uint8)
        self.assertEqual(heat.tolist(), [[0, 127, 255, 255]])


class GrassMaskTest(unittest.TestCase):
    """Which pixels are grass: those the picture loses when the grass is
    switched off (the console's tg) with the head still and no wind."""

    def test_pixels_that_change_with_the_grass_gone_are_grass(self):
        grass = series([[100, 50, 50], [100, 50, 50]])
        bare = series([[20, 50, 53], [20, 50, 53]])
        mask = grass_flicker_map.grass_mask(grass, bare, 8.0)
        self.assertEqual(mask.tolist(), [[True, False, False]])

    def test_a_pixel_unsteady_without_grass_is_no_grass(self):
        # A leaf in the wind differs between any two series; it is unsteady
        # in the one without grass too, and that tells it from the grass.
        grass = series([[100, 100], [100, 100], [100, 100], [100, 100]])
        bare = series([[20, 40], [20, 0], [20, 80], [20, 0]])
        mask = grass_flicker_map.grass_mask(grass, bare, 8.0)
        self.assertEqual(mask.tolist(), [[True, False]])

    def test_series_of_different_sizes_give_no_mask(self):
        grass = series([[100, 50]])
        bare = series([[20, 50, 50]])
        self.assertIsNone(grass_flicker_map.grass_mask(grass, bare, 8.0))

    def test_a_missing_series_gives_no_mask(self):
        self.assertIsNone(grass_flicker_map.grass_mask(None, series([[1]]), 8.0))
        self.assertIsNone(grass_flicker_map.grass_mask(series([[1]]), None, 8.0))


class ZonesTest(unittest.TestCase):
    """The picture in three: grass with grass around it (inside), grass
    within reach of what is no grass - stones, the ground, the wall (edge) -
    and the rest (outside, the grass's reach taken off it too)."""

    def test_a_block_of_grass_has_an_inside_and_an_edge(self):
        mask = np.zeros((7, 7), dtype=bool)
        mask[1:6, 1:6] = True
        inside, edge, outside = grass_flicker_map.zones(mask, 1)
        self.assertEqual(int(inside.sum()), 9)       # the 3x3 core
        self.assertEqual(int(edge.sum()), 16)        # the ring of the block
        self.assertTrue(inside[3, 3])
        self.assertTrue(edge[1, 1])
        self.assertFalse(outside.any())              # every bare pixel is in the grass's reach

    def test_bare_pixels_out_of_the_grass_reach_are_outside(self):
        mask = np.zeros((1, 6), dtype=bool)
        mask[0, 0] = True
        inside, edge, outside = grass_flicker_map.zones(mask, 1)
        self.assertEqual(edge.tolist(), [[True, False, False, False, False, False]])
        self.assertFalse(inside.any())
        self.assertEqual(outside.tolist(), [[False, False, True, True, True, True]])

    def test_the_pictures_rim_is_no_neighbour(self):
        # Grass up to the rim is not at an edge there: nothing bare is seen.
        mask = np.ones((3, 3), dtype=bool)
        inside, edge, outside = grass_flicker_map.zones(mask, 1)
        self.assertTrue(inside.all())
        self.assertFalse(edge.any())
        self.assertFalse(outside.any())

    def test_a_zone_mean_reads_its_pixels_and_an_empty_zone_has_none(self):
        m = np.asarray([[2.0, 4.0, 9.0]], dtype=np.float32)
        zone = np.asarray([[True, True, False]])
        self.assertEqual(grass_flicker_map.zone_mean(m, zone), 3.0)
        self.assertIsNone(grass_flicker_map.zone_mean(m, np.zeros((1, 3), dtype=bool)))


class VerdictTest(unittest.TestCase):
    """Whether a run says anything, and what it says."""

    def read(self, **over):
        values = dict(grass_share=0.2, wind_inside=8.0, wind_edge=8.0, calm_inside=0.5,
                      calm_edge=0.5, outside=0.3)
        values.update(over)
        return grass_flicker_map.verdict(**values)

    def test_too_little_grass_in_view_is_no_measurement(self):
        ok, text = self.read(grass_share=0.01)
        self.assertFalse(ok)
        self.assertIn("grass", text)

    def test_no_mask_is_no_measurement(self):
        ok, _ = self.read(grass_share=None)
        self.assertFalse(ok)

    def test_grass_that_never_changes_in_the_wind_is_a_frozen_picture(self):
        ok, text = self.read(wind_inside=0.0, wind_edge=0.0)
        self.assertFalse(ok)
        self.assertIn("frozen", text)

    def test_a_zone_without_pixels_is_no_measurement(self):
        ok, _ = self.read(wind_edge=None)
        self.assertFalse(ok)

    def test_flicker_that_goes_with_the_wind_is_the_winds(self):
        ok, text = self.read()
        self.assertTrue(ok)
        self.assertIn("wind", text)
        self.assertNotIn("without the wind", text)

    def test_flicker_that_stays_without_the_wind_is_named(self):
        ok, text = self.read(calm_inside=6.0, calm_edge=6.0)
        self.assertTrue(ok)
        self.assertIn("without the wind", text)

    def test_the_edge_flickering_harder_than_the_inside_is_named(self):
        ok, text = self.read(wind_edge=20.0)
        self.assertTrue(ok)
        self.assertIn("edge", text)

    def test_the_inside_flickering_as_hard_as_the_edge_is_named(self):
        ok, text = self.read(wind_edge=8.0, wind_inside=9.0)
        self.assertTrue(ok)
        self.assertIn("all over", text)

    def test_grass_as_steady_as_what_is_beside_it_does_not_flicker(self):
        # The floor every pixel has (the frame's own noise, a shadow that
        # moves) is no flicker of the grass.
        ok, text = self.read(wind_inside=0.3, wind_edge=0.35, calm_inside=0.3, calm_edge=0.3, outside=0.3)
        self.assertTrue(ok)
        self.assertIn("steady", text)
        self.assertNotIn("without the wind", text)


class TurnReadingTest(unittest.TestCase):
    """What the grass adds under a turning head: the same turn with the
    grass switched off is taken away from it."""

    def test_the_turn_without_grass_is_taken_off(self):
        ok, text = grass_flicker_map.turn_reading(turn=3.0, calmturn=2.0, bareturn=1.0)
        self.assertTrue(ok)
        self.assertIn("2.00", text)   # with the wind
        self.assertIn("1.00", text)   # without it, and the bare turn

    def test_grass_that_adds_nothing_is_said_so(self):
        ok, text = grass_flicker_map.turn_reading(turn=1.05, calmturn=1.0, bareturn=1.0)
        self.assertTrue(ok)
        self.assertIn("adds nothing", text)

    def test_a_missing_turn_is_no_measurement(self):
        for missing in ("turn", "calmturn", "bareturn"):
            values = dict(turn=3.0, calmturn=2.0, bareturn=1.0)
            values[missing] = None
            ok, text = grass_flicker_map.turn_reading(**values)
            self.assertFalse(ok)
            self.assertIn(missing, text)


if __name__ == "__main__":
    unittest.main()
