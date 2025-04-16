#pragma once

#include <iostream>

namespace lmt
{
#pragma region Vector2

	class Vector2
	{
	public:
		Vector2() { x = 0; y = 0; }
		Vector2(double X, double Y) { x = X; y = Y; }

		//Basic
		Vector2 operator + (const Vector2& vector) const
		{
			return Vector2(x + vector.x, y + vector.y);
		}

		Vector2 operator - (const Vector2& vector) const
		{
			return Vector2(x - vector.x, y - vector.y);
		}

		Vector2 operator * (const Vector2& vector) const
		{
			return Vector2(x * vector.x, y * vector.y);
		}

		Vector2 operator / (const Vector2& vector) const
		{
			return Vector2(x / vector.x, y / vector.y);
		}

		//Comparison
		bool operator == (const Vector2& vector) const
		{
			if (x == vector.x && y == vector.y)
				return true;
			else
				return false;
		}
		bool operator != (const Vector2& vector) const
		{
			if (x != vector.x && y != vector.y)
				return true;
			else
				return false;
		}
		bool operator > (const Vector2& vector) const
		{
			if (x > vector.x && y > vector.y)
				return true;
			else
				return false;
		}
		bool operator < (const Vector2& vector) const
		{
			if (x < vector.x && y < vector.y)
				return true;
			else
				return false;
		}

		//Assignment
		Vector2& operator += (const Vector2& vector)
		{
			x += vector.x;
			y += vector.y;
			return *this;
		}

		Vector2& operator -= (const Vector2& vector)
		{
			x -= vector.x;
			y -= vector.y;
			return *this;
		}

		Vector2& operator *= (const Vector2& vector)
		{
			x *= vector.x;
			y *= vector.y;
			return *this;
		}

		Vector2& operator /= (const Vector2& vector)
		{
			x /= vector.x;
			y /= vector.y;
			return *this;
		}

		//Unary
		Vector2 operator - () const
		{
			return Vector2(-x, -y);
		}

		//Postfix
		Vector2& operator++ ()
		{
			x += 1;
			y += 1;
			return *this;
		}
		Vector2& operator-- ()
		{
			x -= 1;
			y -= 1;
			return *this;
		}

		double x = 0, y = 0;
	};
#pragma endregion

#pragma region Vector3

	class Vector3
	{
	public:
		Vector3() { x = 0; y = 0; z = 0; }
		Vector3(double X, double Y, double Z) { x = X; y = Y; z = Z; }

		//Basic
		Vector3 operator + (const Vector3& vector) const
		{
			return Vector3(x + vector.x, y + vector.y, z + vector.z);
		}

		Vector3 operator - (const Vector3& vector) const
		{
			return Vector3(x - vector.x, y - vector.y, z - vector.z);
		}

		Vector3 operator * (const Vector3& vector) const
		{
			return Vector3(x * vector.x, y * vector.y, z * vector.z);
		}

		Vector3 operator / (const Vector3& vector) const
		{
			return Vector3(x / vector.x, y / vector.y, z / vector.z);
		}

		//Comparison
		bool operator == (const Vector3& vector) const
		{
			if (x == vector.x && y == vector.y && z == vector.z)
				return true;
			else
				return false;
		}
		bool operator != (const Vector3& vector) const
		{
			if (x != vector.x && y != vector.y && z != vector.z)
				return true;
			else
				return false;
		}
		bool operator > (const Vector3& vector) const
		{
			if (x > vector.x && y > vector.y && z > vector.z)
				return true;
			else
				return false;
		}
		bool operator < (const Vector3& vector) const
		{
			if (x < vector.x && y < vector.y && z < vector.z)
				return true;
			else
				return false;
		}

		//Assignment
		Vector3& operator += (const Vector3& vector)
		{
			x += vector.x;
			y += vector.y;
			z += vector.z;
			return *this;
		}

		Vector3& operator -= (const Vector3& vector)
		{
			x -= vector.x;
			y -= vector.y;
			z -= vector.z;
			return *this;
		}

		Vector3& operator *= (const Vector3& vector)
		{
			x *= vector.x;
			y *= vector.y;
			z *= vector.z;
			return *this;
		}

		Vector3& operator /= (const Vector3& vector)
		{
			x /= vector.x;
			y /= vector.y;
			z /= vector.z;
			return *this;
		}

		//Unary
		Vector3 operator - () const
		{
			return Vector3(-x, -y, -z);
		}

		//Postfix
		Vector3& operator++ ()
		{
			x += 1;
			y += 1;
			z += 1;
			return *this;
		}
		Vector3& operator-- ()
		{
			x -= 1;
			y -= 1;
			z -= 1;
			return *this;
		}

		double x = 0, y = 0, z = 0;
	};
#pragma endregion

#pragma region Vector4

	class Vector4
	{
	public:
		Vector4() { x = 0; y = 0; z = 0; w = 0; }
		Vector4(double X, double Y, double Z, double W) { x = X; y = Y; z = Z; w = W; }

		//Basic
		Vector4 operator + (const Vector4& vector) const
		{
			return Vector4(x + vector.x, y + vector.y, z + vector.z, w + vector.w);
		}

		Vector4 operator - (const Vector4& vector) const
		{
			return Vector4(x - vector.x, y - vector.y, z - vector.z, w - vector.w);
		}

		Vector4 operator * (const Vector4& vector) const
		{
			return Vector4(x * vector.x, y * vector.y, z * vector.z, w * vector.w);
		}

		Vector4 operator / (const Vector4& vector) const
		{
			return Vector4(x / vector.x, y / vector.y, z / vector.z, w / vector.w);
		}

		//Comparison
		bool operator == (const Vector4& vector) const
		{
			if (x == vector.x && y == vector.y && z == vector.z && w == vector.w)
				return true;
			else
				return false;
		}
		bool operator != (const Vector4& vector) const
		{
			if (x != vector.x && y != vector.y && z != vector.z && w != vector.w)
				return true;
			else
				return false;
		}
		bool operator > (const Vector4& vector) const
		{
			if (x > vector.x && y > vector.y && z > vector.z && w > vector.w)
				return true;
			else
				return false;
		}
		bool operator < (const Vector4& vector) const
		{
			if (x < vector.x && y < vector.y && z < vector.z && w < vector.w)
				return true;
			else
				return false;
		}

		//Assignment
		Vector4& operator += (const Vector4& vector)
		{
			x += vector.x;
			y += vector.y;
			z += vector.z;
			w += vector.w;
			return *this;
		}

		Vector4& operator -= (const Vector4& vector)
		{
			x -= vector.x;
			y -= vector.y;
			z -= vector.z;
			w -= vector.w;
			return *this;
		}

		Vector4& operator *= (const Vector4& vector)
		{
			x *= vector.x;
			y *= vector.y;
			z *= vector.z;
			w *= vector.w;
			return *this;
		}

		Vector4& operator /= (const Vector4& vector)
		{
			x /= vector.x;
			y /= vector.y;
			z /= vector.z;
			w /= vector.w;
			return *this;
		}

		//Unary
		Vector4 operator - () const
		{
			return Vector4(-x, -y, -z, -w);
		}

		//Postfix
		Vector4& operator++ ()
		{
			x += 1;
			y += 1;
			z += 1;
			w += 1;
			return *this;
		}
		Vector4& operator-- ()
		{
			x -= 1;
			y -= 1;
			z -= 1;
			w -= 1;
			return *this;
		}

		double x = 0, y = 0, z = 0, w = 0;
	};
#pragma endregion
}