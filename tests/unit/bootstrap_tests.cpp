bool IsValidControlViewport(int width, int height) noexcept;

int main()
{
    if (IsValidControlViewport(0, 480))
    {
        return 1;
    }
    if (IsValidControlViewport(640, 0))
    {
        return 1;
    }
    if (IsValidControlViewport(0, 0))
    {
        return 1;
    }
    if (IsValidControlViewport(-1, 480))
    {
        return 1;
    }
    if (!IsValidControlViewport(640, 480))
    {
        return 1;
    }
    return 0;
}
