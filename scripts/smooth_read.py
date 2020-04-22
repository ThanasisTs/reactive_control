#!/usr/bin/env python
import sys
import rospy
from geometry_msgs.msg import Point
from visualization_msgs.msg import Marker
from scipy.spatial import distance

def main():
	rospy.init_node("points_extraction_from_yaml")
	pub = rospy.Publisher("trajectory_points", Point, queue_size=100)
	pubRaw = rospy.Publisher("vis_raw", Marker, queue_size=100)
	xRaw, yRaw, zRaw = [], [], []
	x, y, z = [], [], []
	file = open(sys.argv[1], 'r')
	fl = file.readlines()
	times = list()
	
	markerRaw = Marker()
	markerRaw.header.frame_id = "base_link"
	markerRaw.header.stamp = rospy.Time.now()
	markerRaw.action = markerRaw.ADD
	markerRaw.type = markerRaw.LINE_STRIP
	markerRaw.pose.position.x = 0
	markerRaw.pose.position.y = 0
	markerRaw.pose.position.z = 0
	markerRaw.pose.orientation.x = 0
	markerRaw.pose.orientation.y = 0
	markerRaw.pose.orientation.z = 0
	markerRaw.pose.orientation.w = 1
	markerRaw.scale.x = 0.01
	markerRaw.color.a = 1.0
	markerRaw.color.r = 1.0
	markerRaw.color.g = 0.0
	markerRaw.color.b = 0.0
	markerRaw.lifetime = rospy.Duration(100)
	first_point = True
	for i in xrange(len(fl)):
		if "x" in fl[i]:
			point = Point()
			point.x = float(fl[i][7:])
			point.y = float(fl[i+1][7:])
			point.z = float(fl[i+2][7:])
			x.append(point.x)
			y.append(point.y)
			z.append(point.z)
			if first_point:
				rospy.sleep(0.5)
				pub.publish(point)
				rospy.loginfo("Published first point")
				rospy.loginfo("Waiting 5 secs")
				rospy.sleep(5)
				first_point = False
			else:
				rospy.sleep(0.047)
				pub.publish(point)
				point.x += 0.6
				point.y += 0.4
				markerRaw.points.append(point)
				pubRaw.publish(markerRaw)
				rospy.loginfo("Published other point")

	print "Published the points"
	for i in xrange(len(x)-1):
		if distance.euclidean([x[i], y[i], z[i]], [x[i+1], y[i+1], z[i+1]]) >= 0.024:
			print i
main()